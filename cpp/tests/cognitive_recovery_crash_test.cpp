#include "core/adapters/episode_segmenter_adapter.hpp"
#include "core/privacy_storage.hpp"
#include "core/runtime_host.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <process.h>
#endif

using namespace eu_digital;

namespace {

CapabilityDescriptor descriptor(std::string operation, std::string id) {
    CapabilityDescriptor value;
    value.capability_id = operation;
    value.implementation_id = std::move(id);
    value.implementation_version = "1.0.0";
    value.kind = "cognitive_adapter";
    value.provides.push_back({std::move(operation), "1.0"});
    value.supports_checkpoint = true;
    return value;
}

std::shared_ptr<EpisodeSegmenterAdapter> register_episode(RuntimeHost& host) {
    auto adapter =
        std::make_shared<EpisodeSegmenterAdapter>("episode-provider");
    host.capability_registry().register_instance<IEpisodeBoundaryPort>(
        descriptor("episode_boundary", "episode-provider"), adapter, 100);
    host.capability_registry().register_instance<ICognitiveStatePort>(
        descriptor("cognitive_state", "episode-state-port"), adapter, 100);
    return adapter;
}

RuntimeConfig config(const std::filesystem::path& timeline, bool snapshots) {
    RuntimeConfig value;
    value.manifest_path =
        (std::filesystem::path(EU_DIGITAL_SOURCE_DIR) /
         "contracts/fixtures/runtime_manifest.json")
            .string();
    value.timeline_path = timeline.string();
    value.session_id = "crash-recovery-session";
    value.observed_at = "2026-08-04T12:10:00Z";
    value.enable_cognitive_snapshots = snapshots;
    value.cognitive_snapshot_interval_events = 2;
    value.cognitive_snapshot_max_age = std::chrono::hours(1);
    return value;
}

CanonicalEvent event(int index) {
    CanonicalEvent value;
    value.event_id = "crash-event-" + std::to_string(index);
    value.source = "system_activity";
    value.event_type = "observation.activity";
    value.payload = "{}";
    value.monotonic_ns = static_cast<std::size_t>(index);
    value.occurred_at = "2026-08-04T12:00:0" + std::to_string(index - 1) + "Z";
    value.received_at = value.occurred_at;
    value.session_id = "episode-session";
    return value;
}

std::string capture(const std::shared_ptr<EpisodeSegmenterAdapter>& adapter) {
    contracts::PortInvocationContextV1 context;
    context.correlation_id = "state-observation";
    context.deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    const auto result = adapter->capture_state(context);
    if (!result.success || !result.value) {
        throw std::runtime_error("cannot capture recovered episode state");
    }
    return result.value->to_json();
}

void write_text(const std::filesystem::path& path, const std::string& value) {
    std::ofstream output(path, std::ios::binary);
    if (!output) throw std::runtime_error("cannot write crash test evidence");
    output << value;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

int child(const std::string& mode, const std::filesystem::path& root) {
    const auto timeline = root /
        (mode == "continuous" ? "continuous.sqlite" : "crash.sqlite");
    RuntimeHost host(config(timeline, mode != "continuous"));
    const auto adapter = register_episode(host);
    if (!host.start()) return 10;

    if (mode == "seed") {
        assert(host.publish(event(1)) == PublishResult::accepted);
        host.coordinator()->wait_idle();
        assert(host.publish(event(2)) == PublishResult::accepted);
        host.coordinator()->wait_idle();
        host.event_bus().wait_idle();
        host.wait_snapshot_idle();
        {
            TimelineStore verify(timeline.string());
            if (verify.load_snapshot_records().size() != 1) return 11;
        }
        assert(host.publish(event(3)) == PublishResult::accepted);
        host.coordinator()->wait_idle();
        host.event_bus().wait_idle();
        std::abort();
    }

    if (mode == "continuous") {
        for (int index = 1; index <= 3; ++index) {
            assert(host.publish(event(index)) == PublishResult::accepted);
            host.coordinator()->wait_idle();
        }
        write_text(root / "continuous.state", capture(adapter));
        host.stop();
        return 0;
    }

    if (mode == "recover") {
        const auto health = host.health_json();
        if (health.find("\"recovered_events\":1") == std::string::npos) {
            return 12;
        }
        const auto recovery = host.cognitive_recovery_json();
        if (recovery.find("\"source\":\"snapshot_replay\"") ==
                std::string::npos ||
            recovery.find("snapshot_decode_or_contract_invalid") ==
                std::string::npos) {
            return 14;
        }
        write_text(root / "recovered.state", capture(adapter));
        host.stop();
        return 0;
    }
    return 13;
}

std::string quote(const std::filesystem::path& value) {
    return "\"" + value.string() + "\"";
}

int run_child(const std::filesystem::path& executable, const char* mode,
              const std::filesystem::path& root) {
#ifdef _WIN32
    const auto executable_text = executable.string();
    const auto root_text = root.string();
    return static_cast<int>(_spawnl(
        _P_WAIT, executable_text.c_str(), executable_text.c_str(), mode,
        root_text.c_str(), nullptr));
#else
    return std::system(
        (quote(executable) + " " + mode + " " + quote(root)).c_str());
#endif
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    if (argc == 3) return child(argv[1], fs::path(argv[2]));
    if (!LocalDataProtection::available()) return 0;

    const auto root = fs::temp_directory_path() /
        "eu_digital_cognitive_recovery_crash_test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto executable = fs::absolute(argv[0]);
    const auto seed = run_child(executable, "seed", root);
    if (seed == 0) {
        std::cerr << "seed process did not abort\n";
        return 20;
    }

    // The newest corrupt record must fall back to the valid pre-crash snapshot.
    {
        TimelineStore store((root / "crash.sqlite").string());
        store.save_snapshot({0xde, 0xad, 0xbe, 0xef}, 999);
    }
    const auto recovered = run_child(executable, "recover", root);
    const auto continuous = run_child(executable, "continuous", root);
    if (recovered != 0 || continuous != 0) return 21;
    if (read_text(root / "recovered.state") !=
        read_text(root / "continuous.state")) {
        std::cerr << "recovered episode state diverged from continuous baseline\n";
        return 22;
    }
    fs::remove_all(root);
    return 0;
}
