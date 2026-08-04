#include "core/cognitive_snapshot.hpp"
#include "core/privacy_storage.hpp"
#include "core/timeline_store.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

using namespace eu_digital;

void test_create_and_verify_checksum() {
    std::cout << "Starting test_create_and_verify_checksum" << std::endl;
    auto snapshot = CognitiveSnapshot::create(
        "2023-10-27T10:00:00Z",
        "test-fingerprint",
        "evt-001",
        "{\"state\":\"active\"}"
    );
    std::cout << "Created snapshot" << std::endl;

    if (snapshot.configuration_fingerprint != "test-fingerprint") {
        std::cerr << "Fingerprint mismatch" << std::endl;
        std::exit(1);
    }
    if (snapshot.last_applied_event_id != "evt-001") {
        std::cerr << "Event ID mismatch" << std::endl;
        std::exit(1);
    }
    if (snapshot.payload_json != "{\"state\":\"active\"}") {
        std::cerr << "Payload mismatch" << std::endl;
        std::exit(1);
    }
    
    // Checksum must be 64 characters (SHA256 hex)
    if (snapshot.checksum.size() != 64) {
        std::cerr << "Checksum size mismatch, size=" << snapshot.checksum.size() << std::endl;
        std::exit(1);
    }
}

void test_save_and_load_fallback() {
    std::cout << "Starting test_save_and_load_fallback" << std::endl;
    if (!LocalDataProtection::available()) {
        std::cout << "Data protection is unavailable, skipping save_and_load_fallback." << std::endl;
        return;
    }
    
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto test_db_path = (std::filesystem::temp_directory_path() / ("cognitive_snapshot_test_" + std::to_string(suffix) + ".db")).string();
    std::cout << "DB path: " << test_db_path << std::endl;

    {
        TimelineStore store(test_db_path);
        std::cout << "Store created" << std::endl;

        // Save first snapshot
        auto snapshot1 = CognitiveSnapshot::create(
            "2023-10-27T10:00:00Z",
            "test-fingerprint",
            "evt-001",
            "{\"state\":\"1\"}"
        );
        std::string json1 = snapshot1.to_json();
        std::vector<std::uint8_t> plain1(json1.begin(), json1.end());
        auto enc1 = LocalDataProtection::protect(plain1);
        std::cout << "Saving snapshot 1" << std::endl;
        store.save_snapshot(enc1, 1000);

        bool rollback_observed = false;
        try {
            store.save_snapshot({}, 1500);
        } catch (const TimelineStoreError&) {
            rollback_observed = true;
        }
        if (!rollback_observed || store.load_snapshot_records().size() != 1) {
            std::cerr << "Expected invalid snapshot transaction rollback" << std::endl;
            std::exit(1);
        }

        // Save second snapshot
        auto snapshot2 = CognitiveSnapshot::create(
            "2023-10-27T11:00:00Z",
            "test-fingerprint",
            "evt-002",
            "{\"state\":\"2\"}"
        );
        std::string json2 = snapshot2.to_json();
        std::vector<std::uint8_t> plain2(json2.begin(), json2.end());
        auto enc2 = LocalDataProtection::protect(plain2);
        std::cout << "Saving snapshot 2" << std::endl;
        store.save_snapshot(enc2, 2000);

        // Load snapshots, verify order (newest first)
        std::cout << "Loading snapshots" << std::endl;
        auto snapshots = store.load_snapshots();
        if (snapshots.size() != 2) {
            std::cerr << "Expected 2 snapshots, got " << snapshots.size() << std::endl;
            std::exit(1);
        }
        const auto records = store.load_snapshot_records();
        if (records.size() != 2 || records[0].created_at_ns != 2000 ||
            records[1].created_at_ns != 1000 || records[0].id <= records[1].id) {
            std::cerr << "Snapshot metadata order mismatch" << std::endl;
            std::exit(1);
        }

        auto dec1 = LocalDataProtection::unprotect(snapshots[0]); // newest
        std::string dec1_str(dec1.begin(), dec1.end());
        if (dec1_str.find("evt-002") == std::string::npos) {
            std::cerr << "Newest snapshot mismatch: " << dec1_str << std::endl;
            std::exit(1);
        }

        auto dec2 = LocalDataProtection::unprotect(snapshots[1]); // oldest
        std::string dec2_str(dec2.begin(), dec2.end());
        if (dec2_str.find("evt-001") == std::string::npos) {
            std::cerr << "Oldest snapshot mismatch: " << dec2_str << std::endl;
            std::exit(1);
        }
    }
    std::filesystem::remove(test_db_path);
}

int main() {
    try {
        test_create_and_verify_checksum();
        test_save_and_load_fallback();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
