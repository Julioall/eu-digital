#include "core/runtime_host.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write test file");
    output << contents;
}

bool contains(const std::string& value, const std::string& expected) {
    return value.find(expected) != std::string::npos;
}

template <typename Callable>
void expect_throw(Callable&& callable) {
    bool thrown = false;
    try {
        callable();
    } catch (const eu_digital::RuntimeHostError&) {
        thrown = true;
    }
    assert(thrown);
}

std::string optional_manifest() {
    return R"json({
  "schema_version":"1.0",
  "runtime_id":"eu-digital-runtime",
  "runtime_version":"0.1.0",
  "build":{"platform":"fixture","compiler":"fixture","profile":"Debug","commit":"fixture","python_runtime_dependency":false},
  "contract_versions":{"canonical_event":"1.0","runtime_health":"1.0","runtime_manifest":"1.0"},
  "promoted_components":[],
  "optional_capabilities":["fixture.optional"]
})json";
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "eu_digital_runtime_host_test";
    fs::remove_all(root);
    fs::create_directories(root);

    try {
        const auto manifest = fs::path(EU_DIGITAL_SOURCE_DIR) / "contracts/fixtures/runtime_manifest.json";
        const auto invalid_manifest = fs::path(EU_DIGITAL_SOURCE_DIR) / "contracts/fixtures/runtime_manifest.invalid.json";
        const auto event = eu_digital::runtime_detail::read_file(
            (fs::path(EU_DIGITAL_SOURCE_DIR) / "contracts/fixtures/canonical_event.json").string());
        const auto timeline = root / "timeline.sqlite";

        eu_digital::RuntimeHost host({manifest.string(), timeline.string(), "runtime-host-session", "2026-07-29T12:00:00Z"});
        assert(host.start());
        assert(host.state() == eu_digital::RuntimeState::ready);
        assert(host.capability_registry().active_profile() == "runtime-host-minimal");
        assert(host.capability_registry().records().empty());
        assert(host.start());
        const auto accepted = host.publish_json(event);
        assert(accepted == eu_digital::PublishResult::accepted);
        assert(host.replay().size() == 1);
        const auto ready_health = host.health_json();
        eu_digital::RuntimeHost::validate_health_json(ready_health);
        assert(contains(ready_health, "\"state\":\"ready\""));
        assert(contains(ready_health, "\"published_events\":1"));
        const auto storage_health = host.storage_health_json();
        eu_digital::RuntimeHost::validate_storage_health_json(storage_health);
        assert(contains(storage_health, "\"quota_bytes\":10737418240"));
        assert(contains(storage_health, "\"status\":\"ready\""));
        host.stop();
        host.stop();
        const auto stopped_health = host.health_json();
        eu_digital::RuntimeHost::validate_health_json(stopped_health);
        assert(contains(stopped_health, "\"state\":\"stopped\""));

        assert(host.start());
        assert(host.replay().size() == 1);
        assert(contains(host.health_json(), "\"recovered_events\":1"));
        host.stop();

        eu_digital::RuntimeHost recovered({manifest.string(), timeline.string(), "recovery-session", "2026-07-29T12:01:00Z"});
        assert(recovered.start());
        assert(recovered.replay().size() == 1);
        recovered.stop();

        const auto deterministic_timeline = root / "deterministic.sqlite";
        const auto run_fixture = [&] {
            eu_digital::RuntimeHost deterministic({manifest.string(), deterministic_timeline.string(), "fixed-session", "2026-07-29T12:04:00Z"});
            assert(deterministic.start());
            assert(deterministic.publish_json(event) == eu_digital::PublishResult::accepted);
            const auto snapshot = deterministic.health_json();
            deterministic.stop();
            return snapshot;
        };
        const auto first_snapshot = run_fixture();
        fs::remove(deterministic_timeline);
        const auto second_snapshot = run_fixture();
        assert(first_snapshot == second_snapshot);

        eu_digital::RuntimeHost invalid({invalid_manifest.string(), (root / "invalid.sqlite").string(), "invalid-session", "2026-07-29T12:02:00Z"});
        assert(!invalid.start());
        assert(invalid.state() == eu_digital::RuntimeState::failed);
        eu_digital::RuntimeHost::validate_health_json(invalid.health_json());

        const auto optional = root / "optional_manifest.json";
        write_file(optional, optional_manifest());
        eu_digital::RuntimeHost degraded({optional.string(), (root / "optional.sqlite").string(), "optional-session", "2026-07-29T12:03:00Z"});
        assert(degraded.start());
        assert(degraded.state() == eu_digital::RuntimeState::degraded);
        const auto degraded_health = degraded.health_json();
        assert(contains(degraded_health, "fixture.optional"));
        assert(contains(degraded_health, "temporarily_unavailable"));
        degraded.stop();

        eu_digital::RuntimeHost unavailable_timeline({manifest.string(), root.string(), "failure-session", "2026-07-29T12:05:00Z"});
        assert(!unavailable_timeline.start());
        assert(unavailable_timeline.state() == eu_digital::RuntimeState::failed);
        assert(contains(unavailable_timeline.health_json(), "runtime_start_failed"));

        expect_throw([] { eu_digital::RuntimeHost::parse_canonical_event("{\"schema_version\":\"1.0\"}"); });
    } catch (...) {
        fs::remove_all(root);
        throw;
    }

    fs::remove_all(root);
    std::cout << "runtime host tests passed\n";
    return 0;
}
