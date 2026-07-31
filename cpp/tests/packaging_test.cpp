#include "core/update_manager.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

static int passed = 0;
static int total = 0;

static void check(bool condition, const char* label) {
    ++total;
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        throw std::runtime_error(label);
    }
    ++passed;
}

static void expect_throw(auto callable, const char* label) {
    ++total;
    try {
        callable();
        std::cerr << "FAIL (no throw): " << label << '\n';
        throw std::runtime_error(label);
    } catch (const eu_digital::PackagingError&) {
        ++passed;
    }
}

static eu_digital::PackageArtifact make_runtime(const std::string& version = "0.2.0") {
    return {
        "eu-digital-runtime",
        eu_digital::PackageKind::runtime,
        version,
        "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2",
        1024 * 1024,  // 1 MiB
        "MIT",
        "windows-x64",
        true
    };
}

static eu_digital::PackageArtifact make_model_payload() {
    return {
        "eu-digital-model-phi3-q4",
        eu_digital::PackageKind::model_payload,
        "1.0.0",
        "b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b200",
        3ULL * 1024 * 1024 * 1024,  // 3 GiB
        "Apache-2.0",
        "windows-x64",
        true
    };
}

static eu_digital::ReleaseManifest make_manifest(const std::string& version = "0.2.0") {
    eu_digital::ReleaseManifest manifest;
    manifest.release_id = "release-" + version;
    manifest.runtime = make_runtime(version);
    manifest.model_payload = make_model_payload();
    manifest.sbom = {{"sqlite3", "3.53.4", "Public Domain", "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234"}};
    manifest.created_at = "2026-07-31T12:00:00+00:00";
    return manifest;
}

static void test_manifest_validation() {
    auto manifest = make_manifest();
    manifest.validate();  // should not throw
    check(true, "valid manifest accepted");
}

static void test_crash_report_no_sensory() {
    auto manifest = make_manifest();
    manifest.crash_reports_contain_sensory = true;
    expect_throw([&] { manifest.validate(); }, "sensory crash reports rejected");
}

static void test_runtime_artifact_validation() {
    eu_digital::PackageArtifact art;
    art.artifact_id = "test";
    art.kind = eu_digital::PackageKind::runtime;
    art.version = "1.0";
    art.sha256_hash = "short";
    art.size_bytes = 100;
    art.license = "MIT";
    expect_throw([&] { art.validate(); }, "short hash rejected");
}

static void test_model_payload_size_limit() {
    eu_digital::PackageArtifact payload;
    payload.artifact_id = "model-too-big";
    payload.kind = eu_digital::PackageKind::model_payload;
    payload.version = "1.0";
    payload.sha256_hash = "a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2";
    payload.size_bytes = 5ULL * 1024 * 1024 * 1024;  // 5 GiB > 4 GiB limit
    payload.license = "Apache-2.0";
    expect_throw([&] { payload.validate(); }, "oversized payload rejected");
}

static void test_update_lifecycle() {
    eu_digital::UpdateManager manager("0.1.0");
    check(manager.current_version() == "0.1.0", "initial version");

    auto manifest = make_manifest("0.2.0");
    auto record = manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");
    check(record.state == eu_digital::UpdateState::downloading, "downloading state");
    check(record.from_version == "0.1.0", "from version");
    check(record.to_version == "0.2.0", "to version");

    record = manager.apply_update(record.update_id, manifest);
    check(record.state == eu_digital::UpdateState::verifying, "verifying state");

    record = manager.complete_update(record.update_id, "2026-07-31T12:05:00+00:00");
    check(record.state == eu_digital::UpdateState::completed, "completed state");
    check(manager.current_version() == "0.2.0", "version updated");
}

static void test_same_version_rejected() {
    eu_digital::UpdateManager manager("0.2.0");
    auto manifest = make_manifest("0.2.0");
    expect_throw([&] {
        manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");
    }, "same version rejected");
}

static void test_rollback() {
    eu_digital::UpdateManager manager("0.1.0");
    auto manifest = make_manifest("0.2.0");
    auto record = manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");

    auto rolled = manager.rollback(record.update_id, "test failure", "2026-07-31T12:01:00+00:00");
    check(rolled.state == eu_digital::UpdateState::rolled_back, "rolled back state");
    check(rolled.failure_reason.value() == "test failure", "rollback reason");
    check(rolled.rollback_version.value() == "0.1.0", "rollback to original");
}

static void test_interrupted_update() {
    eu_digital::UpdateManager manager("0.1.0");
    auto manifest = make_manifest("0.2.0");
    auto record = manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");
    manager.apply_update(record.update_id, manifest);

    // Simulate power failure
    auto failed = manager.handle_interrupted_update(record.update_id, "2026-07-31T12:02:00+00:00");
    check(failed.state == eu_digital::UpdateState::failed, "failed state");
    check(failed.failure_reason.value() == "update_interrupted", "interrupted reason");
    check(manager.current_version() == "0.1.0", "reverted to original");
}

static void test_integrity_verification() {
    eu_digital::UpdateManager manager;
    const std::string data = "hello world";
    const auto expected = eu_digital::digest::hex(eu_digital::digest::sha256(data));
    check(manager.verify_integrity(data, expected), "integrity passes");
    check(!manager.verify_integrity(data, "0000000000000000000000000000000000000000000000000000000000000000"), "wrong hash fails");
}

static void test_corruption_detection() {
    check(eu_digital::UpdateManager::detect_corruption(""), "empty path is corrupt");
    check(!eu_digital::UpdateManager::detect_corruption("/valid/path"), "valid path is not corrupt");
}

static void test_disk_space_check() {
    check(eu_digital::UpdateManager::check_disk_space(1000, 2000), "enough space");
    check(!eu_digital::UpdateManager::check_disk_space(2000, 1000), "not enough space");
}

static void test_crash_report_metadata() {
    eu_digital::UpdateManager manager("0.1.0");
    auto meta = manager.crash_report_metadata();
    check(meta.at("runtime_version") == "0.1.0", "crash report version");
    check(meta.at("contains_sensory_data") == "false", "no sensory data");
    check(meta.at("platform") == "windows-11-x64", "platform");
}

static void test_sbom_validation() {
    eu_digital::SBOMEntry entry;
    entry.name = "";
    entry.version = "1.0";
    entry.license = "MIT";
    expect_throw([&] { entry.validate(); }, "empty SBOM name rejected");
}

static void test_model_payload_optional() {
    eu_digital::ReleaseManifest manifest;
    manifest.release_id = "release-minimal";
    manifest.runtime = make_runtime("0.2.0");
    // No model payload
    manifest.created_at = "2026-07-31T12:00:00+00:00";
    manifest.validate();  // Should succeed without model payload
    check(true, "manifest without model payload is valid");
}

static void test_rollback_completed_rejected() {
    eu_digital::UpdateManager manager("0.1.0");
    auto manifest = make_manifest("0.2.0");
    auto record = manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");
    manager.apply_update(record.update_id, manifest);
    manager.complete_update(record.update_id, "2026-07-31T12:01:00+00:00");
    expect_throw([&] {
        manager.rollback(record.update_id, "too late", "2026-07-31T12:02:00+00:00");
    }, "rollback completed update rejected");
}

static void test_update_history() {
    eu_digital::UpdateManager manager("0.1.0");
    check(manager.history().empty(), "no history initially");
    auto manifest = make_manifest("0.2.0");
    manager.begin_update(manifest, "2026-07-31T12:00:00+00:00");
    check(manager.history().size() == 1, "one history entry");
}

int main() {
    test_manifest_validation();
    test_crash_report_no_sensory();
    test_runtime_artifact_validation();
    test_model_payload_size_limit();
    test_update_lifecycle();
    test_same_version_rejected();
    test_rollback();
    test_interrupted_update();
    test_integrity_verification();
    test_corruption_detection();
    test_disk_space_check();
    test_crash_report_metadata();
    test_sbom_validation();
    test_model_payload_optional();
    test_rollback_completed_rejected();
    test_update_history();

    std::cout << passed << '/' << total << " packaging tests passed\n";
    return passed == total ? 0 : 1;
}
