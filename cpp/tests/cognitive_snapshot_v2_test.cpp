#include "core/cognitive_snapshot.hpp"
#include "core/ports/icognitive_state_port.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>

using namespace eu_digital;

namespace {

contracts::CognitiveStateBundleV1 bundle() {
    contracts::CognitiveStateBundleV1 value;
    value.coordinator.policy_id = "bounded_ports_v1";
    value.coordinator.seen_event_ids = {"event-1"};
    value.required_provider_ids = {"episode_boundary_impl"};
    contracts::CognitiveStateFragmentV1 fragment;
    fragment.provider_id = "episode_boundary_impl";
    fragment.state_schema_version = "1.0";
    fragment.entries = {{"events", "1"}};
    value.fragments.push_back(std::move(fragment));
    return value;
}

std::string fixture() {
    std::ifstream input(
        std::string(EU_DIGITAL_SOURCE_DIR) +
        "/contracts/fixtures/cognitive_snapshot_v2.json",
        std::ios::binary);
    assert(input);
    std::ostringstream contents;
    contents << input.rdbuf();
    auto value = contents.str();
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

class StateFixture final : public ICognitiveStatePort {
public:
    std::string provider_id() const override { return "episode_boundary_impl"; }
    std::string state_schema_version() const override { return "1.0"; }

    contracts::PortResult<contracts::CognitiveStateFragmentV1> capture_state(
        const contracts::PortInvocationContextV1& context) const override {
        if (context.stop_requested()) {
            return contracts::PortResult<contracts::CognitiveStateFragmentV1>::failed(
                "cognitive_state.capture", "cancelled", "cancelled");
        }
        return contracts::PortResult<contracts::CognitiveStateFragmentV1>::ok(
            bundle().fragments.front());
    }

    contracts::PortResult<contracts::CognitiveStateRestoreResultV1> restore_state(
        const contracts::CognitiveStateFragmentV1& fragment,
        const contracts::PortInvocationContextV1&) override {
        if (!fragment.valid() || fragment.provider_id != provider_id()) {
            return contracts::PortResult<contracts::CognitiveStateRestoreResultV1>::failed(
                "cognitive_state.restore", "incompatible_fragment", "invalid fragment");
        }
        contracts::CognitiveStateRestoreResultV1 result;
        result.provider_id = provider_id();
        result.restored_entries = fragment.entries.size();
        return contracts::PortResult<contracts::CognitiveStateRestoreResultV1>::ok(
            std::move(result));
    }
};

}  // namespace

int main() {
    auto state = bundle();
    assert(state.valid());
    auto incomplete = state;
    incomplete.fragments.clear();
    assert(!incomplete.valid());

    const auto snapshot = CognitiveSnapshotV2::create(
        "2026-08-04T12:00:00Z", 1785844800.0,
        std::string(64, 'a'), "event-1", state);
    assert(snapshot.valid());
    assert(snapshot.to_json() == fixture());
    assert(CognitiveSnapshotV2::serialized_checksum_valid(snapshot.to_json()));

    auto corrupted = snapshot.to_json();
    const auto position = corrupted.find("\"events\":\"1\"");
    assert(position != std::string::npos);
    corrupted[position + 10] = '2';
    assert(!CognitiveSnapshotV2::serialized_checksum_valid(corrupted));

    contracts::PortInvocationContextV1 context;
    context.correlation_id = "snapshot-test";
    context.deadline = std::chrono::steady_clock::now() +
                       std::chrono::seconds(1);
    StateFixture port;
    const auto captured = port.capture_state(context);
    assert(captured.valid() && captured.success && captured.value);
    const auto restored = port.restore_state(*captured.value, context);
    assert(restored.valid() && restored.success && restored.value->valid());
}
