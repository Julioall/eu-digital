#pragma once

#include "core/digest.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* PACKAGING_SCHEMA_VERSION = "1.0";
inline constexpr const char* PACKAGING_CREATED_BY = "update_manager.local.v1";
inline constexpr std::uint64_t MAX_PAYLOAD_BYTES = 4ULL * 1024 * 1024 * 1024;  // 4 GiB
inline constexpr std::uint64_t MAX_RUNTIME_RAM_BYTES = 7ULL * 1024 * 1024 * 1024;  // 7 GiB

class PackagingError : public std::runtime_error {
public:
    explicit PackagingError(const std::string& message) : std::runtime_error(message) {}
};

enum class PackageKind { runtime, model_payload };
enum class UpdateState { idle, downloading, applying, verifying, completed, failed, rolled_back };

struct PackageArtifact {
    std::string artifact_id;
    PackageKind kind{PackageKind::runtime};
    std::string version;
    std::string sha256_hash;
    std::uint64_t size_bytes{0};
    std::string license;
    std::string platform{"windows-x64"};
    bool signed_{false};

    void validate() const {
        if (artifact_id.empty()) throw PackagingError("artifact_id is required");
        if (version.empty()) throw PackagingError("version is required");
        if (sha256_hash.empty() || sha256_hash.size() != 64)
            throw PackagingError("sha256_hash must be a 64-character hex string");
        if (size_bytes == 0) throw PackagingError("size_bytes must be positive");
        if (kind == PackageKind::model_payload && size_bytes > MAX_PAYLOAD_BYTES)
            throw PackagingError("model payload exceeds 4 GiB limit");
        if (license.empty()) throw PackagingError("license is required");
    }

    static std::string kind_string(PackageKind k) {
        switch (k) {
        case PackageKind::runtime: return "runtime";
        case PackageKind::model_payload: return "model_payload";
        }
        throw PackagingError("unknown package kind");
    }
};

struct SBOMEntry {
    std::string name;
    std::string version;
    std::string license;
    std::string sha256_hash;

    void validate() const {
        if (name.empty()) throw PackagingError("SBOM entry name is required");
        if (version.empty()) throw PackagingError("SBOM entry version is required");
        if (license.empty()) throw PackagingError("SBOM entry license is required");
    }
};

struct ReleaseManifest {
    std::string release_id;
    std::string schema_version{PACKAGING_SCHEMA_VERSION};
    std::string platform{"windows-11-x64"};
    PackageArtifact runtime;
    std::optional<PackageArtifact> model_payload;
    std::vector<SBOMEntry> sbom;
    std::vector<std::string> authorized_dynamic_deps;
    bool crash_reports_contain_sensory{false};  // must always be false
    std::string created_at;

    void validate() const {
        if (release_id.empty()) throw PackagingError("release_id is required");
        if (schema_version != PACKAGING_SCHEMA_VERSION)
            throw PackagingError("unsupported packaging schema version");
        runtime.validate();
        if (runtime.kind != PackageKind::runtime)
            throw PackagingError("runtime artifact must be of kind runtime");
        if (model_payload) {
            model_payload->validate();
            if (model_payload->kind != PackageKind::model_payload)
                throw PackagingError("model payload must be of kind model_payload");
        }
        if (crash_reports_contain_sensory)
            throw PackagingError("crash reports must not contain sensory content");
        if (created_at.empty()) throw PackagingError("created_at is required");
        for (const auto& entry : sbom) entry.validate();
    }
};

struct UpdateRecord {
    std::string update_id;
    std::string from_version;
    std::string to_version;
    UpdateState state{UpdateState::idle};
    std::optional<std::string> failure_reason;
    std::optional<std::string> rollback_version;
    std::string started_at;
    std::optional<std::string> completed_at;

    static std::string state_string(UpdateState s) {
        switch (s) {
        case UpdateState::idle: return "idle";
        case UpdateState::downloading: return "downloading";
        case UpdateState::applying: return "applying";
        case UpdateState::verifying: return "verifying";
        case UpdateState::completed: return "completed";
        case UpdateState::failed: return "failed";
        case UpdateState::rolled_back: return "rolled_back";
        }
        throw PackagingError("unknown update state");
    }
};

/// SPEC-044: Update manager with rollback, integrity and recovery.
class UpdateManager {
public:
    explicit UpdateManager(std::string current_version = "0.1.0")
        : current_version_(std::move(current_version)) {
        if (current_version_.empty())
            throw PackagingError("current version is required");
    }

    const std::string& current_version() const { return current_version_; }
    const std::vector<UpdateRecord>& history() const { return history_; }

    /// Validate a release manifest before applying.
    void validate_release(const ReleaseManifest& manifest) const {
        manifest.validate();
    }

    /// Validate artifact integrity against expected hash.
    bool verify_integrity(const std::string& data, const std::string& expected_hash) const {
        const auto actual = digest::hex(digest::sha256(data));
        return actual == expected_hash;
    }

    /// Start an update. Returns the update record.
    UpdateRecord begin_update(const ReleaseManifest& manifest, const std::string& now) {
        manifest.validate();
        if (manifest.runtime.version == current_version_)
            throw PackagingError("target version is the same as current");

        UpdateRecord record;
        record.update_id = digest::uuid5(
            "c1a2b3d4-e5f6-7890-abcd-ef1234567890",
            current_version_ + ":" + manifest.runtime.version + ":" + now);
        record.from_version = current_version_;
        record.to_version = manifest.runtime.version;
        record.state = UpdateState::downloading;
        record.started_at = now;

        history_.push_back(record);
        return record;
    }

    /// Simulate applying the update (in real code, this would replace binaries).
    UpdateRecord apply_update(const std::string& update_id, const ReleaseManifest& manifest) {
        auto* record = find_record(update_id);
        if (!record) throw PackagingError("update record not found");
        if (record->state != UpdateState::downloading)
            throw PackagingError("update must be in downloading state to apply");

        manifest.validate();
        record->state = UpdateState::applying;
        // Simulate verification
        record->state = UpdateState::verifying;
        return *record;
    }

    /// Complete the update.
    UpdateRecord complete_update(const std::string& update_id, const std::string& now) {
        auto* record = find_record(update_id);
        if (!record) throw PackagingError("update record not found");
        if (record->state != UpdateState::verifying)
            throw PackagingError("update must be in verifying state to complete");

        record->state = UpdateState::completed;
        record->completed_at = now;
        previous_version_ = current_version_;
        current_version_ = record->to_version;
        return *record;
    }

    /// Rollback to previous version.
    UpdateRecord rollback(const std::string& update_id, const std::string& reason,
                          const std::string& now) {
        auto* record = find_record(update_id);
        if (!record) throw PackagingError("update record not found");
        if (record->state == UpdateState::completed || record->state == UpdateState::rolled_back)
            throw PackagingError("cannot rollback a completed or already rolled-back update");

        record->state = UpdateState::rolled_back;
        record->failure_reason = reason;
        record->rollback_version = record->from_version;
        record->completed_at = now;
        return *record;
    }

    /// Handle interrupted update (simulate power failure during apply).
    UpdateRecord handle_interrupted_update(const std::string& update_id, const std::string& now) {
        auto* record = find_record(update_id);
        if (!record) throw PackagingError("update record not found");

        record->state = UpdateState::failed;
        record->failure_reason = "update_interrupted";
        record->completed_at = now;

        // Auto-rollback to from_version
        current_version_ = record->from_version;
        return *record;
    }

    /// Handle corrupted database scenario.
    static bool detect_corruption(const std::string& db_path) {
        // In real implementation, would check SQLite integrity
        // Here we check if path is empty or file doesn't conceptually exist
        return db_path.empty();
    }

    /// Handle disk full scenario.
    static bool check_disk_space(std::uint64_t required_bytes, std::uint64_t available_bytes) {
        return available_bytes >= required_bytes;
    }

    /// Generate crash report metadata (no sensory content).
    std::map<std::string, std::string> crash_report_metadata() const {
        return {
            {"runtime_version", current_version_},
            {"contains_sensory_data", "false"},
            {"platform", "windows-11-x64"},
            {"schema_version", PACKAGING_SCHEMA_VERSION},
        };
    }

private:
    UpdateRecord* find_record(const std::string& update_id) {
        for (auto& record : history_) {
            if (record.update_id == update_id) return &record;
        }
        return nullptr;
    }

    std::string current_version_;
    std::optional<std::string> previous_version_;
    std::vector<UpdateRecord> history_;
};

}  // namespace eu_digital
