#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#endif

namespace eu_digital {

class PrivacyStorageError : public std::runtime_error {
public:
    explicit PrivacyStorageError(const std::string& message) : std::runtime_error(message) {}
};

enum class ConsentDecision { grant, revoke };

inline std::string consent_decision_name(ConsentDecision decision) {
    return decision == ConsentDecision::grant ? "grant" : "revoke";
}

struct ConsentRecord {
    std::string sensor_id;
    std::string purpose;
    ConsentDecision decision{ConsentDecision::revoke};
    std::string consent_version;
    std::string decided_at;
};

struct ConsentCheck {
    bool allowed{false};
    std::string reason_code;
};

class LocalDataProtection {
public:
    static bool available() noexcept {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    static std::vector<std::uint8_t> protect(std::span<const std::uint8_t> plaintext) {
#ifdef _WIN32
        DATA_BLOB input{
            static_cast<DWORD>(plaintext.size()),
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plaintext.data()))};
        DATA_BLOB output{};
        if (!CryptProtectData(&input, L"eu-digital-local-data", nullptr, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            throw PrivacyStorageError("Windows DPAPI protect failed: " + std::to_string(GetLastError()));
        }
        std::vector<std::uint8_t> protected_data(output.pbData, output.pbData + output.cbData);
        LocalFree(output.pbData);
        return protected_data;
#else
        (void)plaintext;
        throw PrivacyStorageError("Windows DPAPI is unavailable on this platform");
#endif
    }

    static std::vector<std::uint8_t> unprotect(std::span<const std::uint8_t> protected_data) {
#ifdef _WIN32
        DATA_BLOB input{
            static_cast<DWORD>(protected_data.size()),
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(protected_data.data()))};
        DATA_BLOB output{};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                                CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            throw PrivacyStorageError("Windows DPAPI unprotect failed: " + std::to_string(GetLastError()));
        }
        std::vector<std::uint8_t> plaintext(output.pbData, output.pbData + output.cbData);
        LocalFree(output.pbData);
        return plaintext;
#else
        (void)protected_data;
        throw PrivacyStorageError("Windows DPAPI is unavailable on this platform");
#endif
    }
};

class ConsentLedger {
public:
    void grant(std::string sensor_id, std::string purpose,
               std::string consent_version, std::string decided_at) {
        record({std::move(sensor_id), std::move(purpose), ConsentDecision::grant,
                std::move(consent_version), std::move(decided_at)});
    }

    void revoke(std::string sensor_id, std::string purpose,
                std::string consent_version, std::string decided_at) {
        record({std::move(sensor_id), std::move(purpose), ConsentDecision::revoke,
                std::move(consent_version), std::move(decided_at)});
    }

    void record(ConsentRecord value) {
        if (value.sensor_id.empty() || value.purpose.empty() ||
            value.consent_version.empty() || value.decided_at.empty()) {
            throw PrivacyStorageError("consent record fields must not be empty");
        }
        records_.push_back(std::move(value));
    }

    void pause_global() noexcept { global_pause_ = true; }
    void resume_global() noexcept { global_pause_ = false; }
    bool global_paused() const noexcept { return global_pause_; }

    ConsentCheck capture_allowed(const std::string& sensor_id,
                                 const std::string& purpose) const {
        if (global_pause_) return {false, "global_pause"};
        for (auto iterator = records_.rbegin(); iterator != records_.rend(); ++iterator) {
            if (iterator->sensor_id == sensor_id && iterator->purpose == purpose) {
                return iterator->decision == ConsentDecision::grant
                    ? ConsentCheck{true, "consent_granted"}
                    : ConsentCheck{false, "consent_revoked"};
            }
        }
        return {false, "consent_not_granted"};
    }

    const std::vector<ConsentRecord>& records() const noexcept { return records_; }

    std::string serialize() const {
        std::ostringstream output;
        output << "EU_DIGITAL_CONSENT_LEDGER 1\n"
               << "global " << (global_pause_ ? 1 : 0) << '\n';
        for (const auto& value : records_) {
            output << "record " << std::quoted(value.sensor_id) << ' '
                   << std::quoted(value.purpose) << ' '
                   << std::quoted(consent_decision_name(value.decision)) << ' '
                   << std::quoted(value.consent_version) << ' '
                   << std::quoted(value.decided_at) << '\n';
        }
        output << "end\n";
        return output.str();
    }

    static ConsentLedger deserialize(const std::string& serialized) {
        std::istringstream input(serialized);
        std::string header;
        int version = 0;
        if (!(input >> header >> version) || header != "EU_DIGITAL_CONSENT_LEDGER" || version != 1) {
            throw PrivacyStorageError("unsupported consent ledger snapshot");
        }
        ConsentLedger ledger;
        std::string tag;
        while (input >> tag && tag != "end") {
            if (tag == "global") {
                int paused = 0;
                if (!(input >> paused) || (paused != 0 && paused != 1)) {
                    throw PrivacyStorageError("invalid consent global pause state");
                }
                ledger.global_pause_ = paused == 1;
            } else if (tag == "record") {
                std::string sensor_id, purpose, decision, consent_version, decided_at;
                if (!(input >> std::quoted(sensor_id) >> std::quoted(purpose) >>
                      std::quoted(decision) >> std::quoted(consent_version) >>
                      std::quoted(decided_at))) {
                    throw PrivacyStorageError("invalid consent record");
                }
                if (decision != "grant" && decision != "revoke") {
                    throw PrivacyStorageError("invalid consent decision");
                }
                ledger.record({std::move(sensor_id), std::move(purpose),
                               decision == "grant" ? ConsentDecision::grant : ConsentDecision::revoke,
                               std::move(consent_version), std::move(decided_at)});
            } else {
                throw PrivacyStorageError("unknown consent ledger record");
            }
        }
        return ledger;
    }

    void save_encrypted(const std::filesystem::path& path) const {
        const auto serialized = serialize();
        const auto plaintext = std::vector<std::uint8_t>(serialized.begin(), serialized.end());
        const auto ciphertext = LocalDataProtection::protect(plaintext);
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        const auto temporary = path.string() + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw PrivacyStorageError("cannot open encrypted consent snapshot");
        output.write(reinterpret_cast<const char*>(ciphertext.data()),
                     static_cast<std::streamsize>(ciphertext.size()));
        output.close();
        if (!output) throw PrivacyStorageError("cannot write encrypted consent snapshot");
#ifdef _WIN32
        if (!MoveFileExW(std::filesystem::path(temporary).wstring().c_str(), path.wstring().c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary);
            throw PrivacyStorageError("cannot commit encrypted consent snapshot: " + std::to_string(GetLastError()));
        }
#else
        std::error_code error;
        std::filesystem::rename(temporary, path, error);
        if (error) {
            std::filesystem::remove(temporary);
            throw PrivacyStorageError("cannot commit encrypted consent snapshot: " + error.message());
        }
#endif
    }

    static ConsentLedger load_encrypted(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw PrivacyStorageError("cannot open encrypted consent snapshot");
        const std::vector<std::uint8_t> ciphertext{
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto plaintext = LocalDataProtection::unprotect(ciphertext);
        return deserialize(std::string(plaintext.begin(), plaintext.end()));
    }

private:
    bool global_pause_{false};
    std::vector<ConsentRecord> records_;
};

struct RetentionDefaults {
    std::string policy_id{"storage.local.defaults"};
    std::string policy_version{"1"};
    std::uint32_t raw_event_retention_days{30};
    std::uint32_t derived_memory_retention_days{365};
    std::uint32_t quarantine_retention_days{14};
    std::uint32_t user_storage_quota_gib{10};

    std::uint64_t quota_bytes() const {
        constexpr std::uint64_t gib = 1024ULL * 1024ULL * 1024ULL;
        if (user_storage_quota_gib > std::numeric_limits<std::uint64_t>::max() / gib) {
            throw PrivacyStorageError("storage quota overflows byte representation");
        }
        return static_cast<std::uint64_t>(user_storage_quota_gib) * gib;
    }
};

struct StorageUsage {
    std::uint64_t database_bytes{0};
    std::uint64_t wal_bytes{0};
    std::uint64_t index_bytes{0};
    std::uint64_t quarantine_bytes{0};
    std::uint64_t backup_bytes{0};
    std::uint64_t payload_bytes{0};
    std::uint64_t model_bytes{0};

    std::uint64_t user_bytes() const {
        const auto values = {database_bytes, wal_bytes, index_bytes,
                             quarantine_bytes, backup_bytes, payload_bytes};
        std::uint64_t total = 0;
        for (const auto value : values) {
            if (value > std::numeric_limits<std::uint64_t>::max() - total) {
                throw PrivacyStorageError("storage usage overflows byte representation");
            }
            total += value;
        }
        return total;
    }
};

enum class StorageStatus { ready, degraded };

struct StorageHealth {
    StorageStatus status{StorageStatus::ready};
    std::uint64_t quota_bytes{0};
    std::uint64_t user_bytes{0};
    std::uint64_t model_bytes{0};
    bool capture_suspended{false};
    bool user_decision_required{false};
    std::string reason_code;
};

class StorageQuotaController {
public:
    explicit StorageQuotaController(RetentionDefaults policy = {}, StorageUsage usage = {})
        : policy_(std::move(policy)), usage_(usage) {
        refresh_health();
    }

    bool begin_write(std::uint64_t bytes) {
        const auto current = usage_.user_bytes();
        if (bytes > policy_.quota_bytes() || current > policy_.quota_bytes() - bytes ||
            reserved_bytes_ > policy_.quota_bytes() - current - bytes) {
            health_.status = StorageStatus::degraded;
            health_.capture_suspended = true;
            health_.user_decision_required = true;
            health_.reason_code = "storage_quota_exceeded";
            return false;
        }
        reserved_bytes_ += bytes;
        return true;
    }

    void commit_write(StorageUsage delta) {
        const auto committed = delta.user_bytes();
        if (committed > reserved_bytes_) throw PrivacyStorageError("storage commit exceeds reservation");
        reserved_bytes_ -= committed;
        const auto add = [](std::uint64_t& target, std::uint64_t value) {
            if (value > std::numeric_limits<std::uint64_t>::max() - target) {
                throw PrivacyStorageError("storage usage overflows byte representation");
            }
            target += value;
        };
        add(usage_.database_bytes, delta.database_bytes);
        add(usage_.wal_bytes, delta.wal_bytes);
        add(usage_.index_bytes, delta.index_bytes);
        add(usage_.quarantine_bytes, delta.quarantine_bytes);
        add(usage_.backup_bytes, delta.backup_bytes);
        add(usage_.payload_bytes, delta.payload_bytes);
        add(usage_.model_bytes, delta.model_bytes);
        refresh_health();
    }

    void abort_write(std::uint64_t bytes) {
        if (bytes > reserved_bytes_) throw PrivacyStorageError("storage abort exceeds reservation");
        reserved_bytes_ -= bytes;
    }

    void set_usage(StorageUsage usage) {
        usage_ = usage;
        refresh_health();
    }

    bool resolve_user_decision(bool allow_capture) {
        if (!allow_capture) return false;
        if (usage_.user_bytes() > policy_.quota_bytes()) return false;
        health_.status = StorageStatus::ready;
        health_.capture_suspended = false;
        health_.user_decision_required = false;
        health_.reason_code.clear();
        return true;
    }

    const RetentionDefaults& policy() const noexcept { return policy_; }
    const StorageUsage& usage() const noexcept { return usage_; }
    const StorageHealth& health() const noexcept { return health_; }
    std::uint64_t reserved_bytes() const noexcept { return reserved_bytes_; }

private:
    void refresh_health() {
        health_.quota_bytes = policy_.quota_bytes();
        health_.user_bytes = usage_.user_bytes();
        health_.model_bytes = usage_.model_bytes;
        if (health_.user_bytes > health_.quota_bytes) {
            health_.status = StorageStatus::degraded;
            health_.capture_suspended = true;
            health_.user_decision_required = true;
            health_.reason_code = "storage_quota_exceeded";
        }
    }

    RetentionDefaults policy_;
    StorageUsage usage_;
    StorageHealth health_;
    std::uint64_t reserved_bytes_{0};
};

struct StorageOperationResult {
    bool completed{false};
    std::uint64_t bytes{0};
    std::string reason_code;
};

class LocalStorageOperations {
public:
    explicit LocalStorageOperations(std::filesystem::path root)
        : root_(std::filesystem::absolute(std::move(root)).lexically_normal()) {}

    StorageOperationResult export_scope(const std::filesystem::path& relative_source,
                                        const std::filesystem::path& destination) const {
        const auto source = inside_root(relative_source, false);
        if (!std::filesystem::is_regular_file(source)) {
            throw PrivacyStorageError("export source must be a local regular file");
        }
        if (std::filesystem::exists(destination)) {
            throw PrivacyStorageError("export destination already exists");
        }
        const auto bytes = std::filesystem::file_size(source);
        std::filesystem::create_directories(destination.parent_path());
        if (!std::filesystem::copy_file(source, destination)) {
            throw PrivacyStorageError("cannot export local data");
        }
        if (std::filesystem::file_size(destination) != bytes) {
            throw PrivacyStorageError("export verification failed");
        }
        return {true, bytes, "export_completed"};
    }

    StorageOperationResult delete_scope(const std::filesystem::path& relative_target,
                                        bool confirmed) const {
        if (!confirmed) return {false, 0, "user_confirmation_required"};
        const auto target = inside_root(relative_target, false);
        if (!std::filesystem::exists(target)) return {true, 0, "already_absent"};
        const auto bytes = measure_bytes(target);
        std::error_code error;
        std::filesystem::remove_all(target, error);
        if (error) throw PrivacyStorageError("cannot delete requested local data: " + error.message());
        return {true, bytes, "delete_completed"};
    }

    StorageOperationResult recover_file(const std::filesystem::path& relative_target,
                                        const std::filesystem::path& relative_backup,
                                        bool confirmed) const {
        if (!confirmed) return {false, 0, "user_confirmation_required"};
        const auto target = inside_root(relative_target, false);
        const auto backup = inside_root(relative_backup, false);
        if (!std::filesystem::is_regular_file(backup)) {
            throw PrivacyStorageError("recovery backup is missing");
        }
        const auto temporary = target.string() + ".recovery.tmp";
        const auto quarantine = target.string() + ".corrupt";
        if (std::filesystem::exists(temporary) || std::filesystem::exists(quarantine)) {
            throw PrivacyStorageError("recovery staging path already exists");
        }
        std::filesystem::create_directories(target.parent_path());
        std::filesystem::copy_file(backup, temporary);
        const auto bytes = std::filesystem::file_size(temporary);
        if (bytes != std::filesystem::file_size(backup)) {
            std::filesystem::remove(temporary);
            throw PrivacyStorageError("recovery copy verification failed");
        }
        bool quarantined = false;
        try {
            if (std::filesystem::exists(target)) {
                std::filesystem::rename(target, quarantine);
                quarantined = true;
            }
            std::filesystem::rename(temporary, target);
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            if (quarantined) std::filesystem::rename(quarantine, target, ignored);
            throw;
        }
        return {true, bytes, "recovery_completed"};
    }

private:
    std::filesystem::path inside_root(const std::filesystem::path& relative,
                                      bool allow_root) const {
        if (relative.is_absolute()) throw PrivacyStorageError("storage path must be relative");
        const auto candidate = (root_ / relative).lexically_normal();
        const auto relative_to_root = candidate.lexically_relative(root_);
        const auto relative_text = relative_to_root.generic_string();
        if (relative_text.empty() || relative_text == ".." || relative_text.starts_with("../")) {
            throw PrivacyStorageError("storage path escapes the local data root");
        }
        if (!allow_root && candidate == root_) throw PrivacyStorageError("operation on storage root is not allowed");
        return candidate;
    }

    static std::uint64_t measure_bytes(const std::filesystem::path& path) {
        if (std::filesystem::is_regular_file(path)) return std::filesystem::file_size(path);
        std::uint64_t total = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (std::filesystem::is_regular_file(entry.path())) {
                const auto bytes = std::filesystem::file_size(entry.path());
                if (bytes > std::numeric_limits<std::uint64_t>::max() - total) {
                    throw PrivacyStorageError("storage measurement overflows byte representation");
                }
                total += bytes;
            }
        }
        return total;
    }

    std::filesystem::path root_;
};

}  // namespace eu_digital
