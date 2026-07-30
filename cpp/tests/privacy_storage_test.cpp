#include "core/privacy_storage.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using eu_digital::ConsentLedger;
using eu_digital::LocalDataProtection;
using eu_digital::LocalStorageOperations;
using eu_digital::PrivacyStorageError;
using eu_digital::RetentionDefaults;
using eu_digital::StorageQuotaController;
using eu_digital::StorageStatus;
using eu_digital::StorageUsage;

int main() {
    ConsentLedger consent;
    assert(!consent.capture_allowed("system_activity", "local_activity_observation").allowed);
    consent.grant("system_activity", "local_activity_observation", "1", "2026-07-29T12:00:00Z");
    assert(consent.capture_allowed("system_activity", "local_activity_observation").allowed);
    consent.revoke("system_activity", "local_activity_observation", "2", "2026-07-29T12:01:00Z");
    assert(!consent.capture_allowed("system_activity", "local_activity_observation").allowed);
    consent.grant("system_activity", "local_activity_observation", "3", "2026-07-29T12:02:00Z");
    consent.pause_global();
    assert(!consent.capture_allowed("system_activity", "local_activity_observation").allowed);
    consent.resume_global();
    assert(consent.capture_allowed("system_activity", "local_activity_observation").allowed);
    const auto serialized = consent.serialize();
    const auto restored = ConsentLedger::deserialize(serialized);
    assert(restored.records().size() == 3);
    assert(restored.capture_allowed("system_activity", "local_activity_observation").allowed);

    RetentionDefaults policy;
    assert(policy.raw_event_retention_days == 30);
    assert(policy.derived_memory_retention_days == 365);
    assert(policy.quarantine_retention_days == 14);
    assert(policy.user_storage_quota_gib == 10);
    assert(policy.quota_bytes() == 10ULL * 1024ULL * 1024ULL * 1024ULL);

    StorageQuotaController quota(policy, StorageUsage{.database_bytes = policy.quota_bytes() - 10});
    assert(quota.begin_write(10));
    quota.commit_write(StorageUsage{.payload_bytes = 10});
    assert(quota.health().status == StorageStatus::ready);
    assert(!quota.begin_write(1));
    assert(quota.health().capture_suspended);
    assert(quota.health().user_decision_required);
    assert(quota.health().reason_code == "storage_quota_exceeded");
    quota.set_usage(StorageUsage{.database_bytes = policy.quota_bytes() - 1});
    assert(quota.resolve_user_decision(true));
    assert(!quota.health().capture_suspended);

    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("eu-digital-spec030-" + std::to_string(suffix));
    const auto export_path = std::filesystem::temp_directory_path() /
        ("eu-digital-spec030-export-" + std::to_string(suffix) + ".json");
    std::filesystem::create_directories(root / "data");
    {
        std::ofstream data(root / "data" / "events.json");
        data << "local event data";
    }
    LocalStorageOperations operations(root);
    const auto exported = operations.export_scope("data/events.json", export_path);
    assert(exported.completed && exported.bytes > 0);
    assert(std::filesystem::exists(export_path));
    assert(!operations.delete_scope("data/events.json", false).completed);
    assert(operations.delete_scope("data/events.json", true).completed);

    {
        std::ofstream backup(root / "data" / "events.backup");
        backup << "known good";
        std::ofstream damaged(root / "data" / "events.json");
        damaged << "damaged";
    }
    assert(operations.recover_file("data/events.json", "data/events.backup", true).completed);
    std::ifstream recovered(root / "data" / "events.json");
    std::string recovered_content;
    recovered >> recovered_content >> std::ws;
    assert(recovered_content == "known");
    assert(std::filesystem::exists(root / "data" / "events.json.corrupt"));
    recovered.close();

    try {
        (void)operations.delete_scope("../outside", true);
        assert(false);
    } catch (const PrivacyStorageError&) {
        assert(true);
    }

#ifdef _WIN32
    const std::vector<std::uint8_t> plaintext{'l', 'o', 'c', 'a', 'l'};
    const auto protected_data = LocalDataProtection::protect(plaintext);
    assert(LocalDataProtection::unprotect(protected_data) == plaintext);
#else
    assert(!LocalDataProtection::available());
    try {
        const std::vector<std::uint8_t> plaintext{'l'};
        (void)LocalDataProtection::protect(plaintext);
        assert(false);
    } catch (const PrivacyStorageError&) {
        assert(true);
    }
#endif

    std::filesystem::remove_all(root);
    std::filesystem::remove(export_path);
}
