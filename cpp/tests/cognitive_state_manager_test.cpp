#include "core/cognitive_state_manager.hpp"

#include <cassert>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using namespace eu_digital;

namespace {

class StatePort final : public ICognitiveStatePort {
public:
    StatePort(std::string id, std::string value)
        : id_(std::move(id)), value_(std::move(value)) {}

    std::string provider_id() const override { return id_; }
    std::string state_schema_version() const override { return "1.0"; }

    contracts::PortResult<contracts::CognitiveStateFragmentV1> capture_state(
        const contracts::PortInvocationContextV1&) const override {
        if (capture_delay_ > std::chrono::milliseconds::zero()) {
            std::this_thread::sleep_for(capture_delay_);
        }
        contracts::CognitiveStateFragmentV1 fragment;
        fragment.provider_id = id_;
        fragment.state_schema_version = "1.0";
        fragment.entries = {{"value", value_}};
        return contracts::PortResult<
            contracts::CognitiveStateFragmentV1>::ok(std::move(fragment));
    }

    contracts::PortResult<contracts::CognitiveStateRestoreResultV1> restore_state(
        const contracts::CognitiveStateFragmentV1& fragment,
        const contracts::PortInvocationContextV1&) override {
        if (fragment.provider_id != id_ || !fragment.entries.contains("value") ||
            (fail_value_ && fragment.entries.at("value") == *fail_value_)) {
            return contracts::PortResult<
                contracts::CognitiveStateRestoreResultV1>::failed(
                    "cognitive_state.restore", "fixture_failure", "rejected");
        }
        value_ = fragment.entries.at("value");
        contracts::CognitiveStateRestoreResultV1 result;
        result.provider_id = id_;
        result.restored_entries = fragment.entries.size();
        return contracts::PortResult<
            contracts::CognitiveStateRestoreResultV1>::ok(std::move(result));
    }

    void fail_on(std::string value) { fail_value_ = std::move(value); }
    void delay_capture(std::chrono::milliseconds value) {
        capture_delay_ = value;
    }
    const std::string& value() const { return value_; }

private:
    std::string id_;
    std::string value_;
    std::optional<std::string> fail_value_;
    std::chrono::milliseconds capture_delay_{0};
};

CapabilityDescriptor descriptor(std::string operation, std::string id,
                                bool checkpoint = true) {
    CapabilityDescriptor value;
    value.capability_id = operation;
    value.implementation_id = std::move(id);
    value.implementation_version = "1.0.0";
    value.kind = "fixture";
    value.provides.push_back({std::move(operation), "1.0"});
    value.supports_checkpoint = checkpoint;
    return value;
}

void register_state_port(CapabilityRegistry& registry,
                         const std::shared_ptr<StatePort>& port,
                         const std::string& registration_id) {
    registry.register_instance<ICognitiveStatePort>(
        descriptor("cognitive_state", registration_id), port);
}

}  // namespace

int main() {
    CapabilityRegistry registry;
    auto episode_marker = std::make_shared<int>(1);
    auto workspace_marker = std::make_shared<int>(2);
    registry.register_instance(
        descriptor("episode_boundary", "episode-provider"), episode_marker);
    registry.register_instance(
        descriptor("workspace", "workspace-provider"), workspace_marker);
    auto episode = std::make_shared<StatePort>("episode-provider", "one");
    auto workspace = std::make_shared<StatePort>("workspace-provider", "two");
    register_state_port(registry, episode, "episode-state-port");

    CognitiveCoordinatorConfig config;
    config.auto_start = false;
    CognitiveCoordinator coordinator(registry, config);
    CognitiveStateManager manager(registry, coordinator);
    assert(!manager.capture("2026-08-04T12:00:00Z", 1.0, "fingerprint",
                            "event-1"));

    register_state_port(registry, workspace, "workspace-state-port");
    const auto snapshot = manager.capture(
        "2026-08-04T12:00:00Z", 1.0, "fingerprint", "event-1");
    assert(snapshot && snapshot->valid());
    assert((snapshot->state.required_provider_ids ==
            std::vector<std::string>{"episode-provider", "workspace-provider"}));

    auto desired = snapshot->state;
    desired.fragments[0].entries["value"] = "ten";
    desired.fragments[1].entries["value"] = "twenty";
    assert(manager.restore(desired, "restore-ok"));
    assert(episode->value() == "ten");
    assert(workspace->value() == "twenty");

    auto rejected = desired;
    rejected.fragments[0].entries["value"] = "rollback-me";
    rejected.fragments[1].entries["value"] = "reject-me";
    workspace->fail_on("reject-me");
    assert(!manager.restore(rejected, "restore-fail"));
    assert(episode->value() == "ten");
    assert(workspace->value() == "twenty");

    CapabilityRegistry incomplete;
    incomplete.register_instance(
        descriptor("episode_boundary", "no-checkpoint", false),
        std::make_shared<int>(3));
    CognitiveCoordinator incomplete_coordinator(incomplete, config);
    CognitiveStateManager incomplete_manager(incomplete, incomplete_coordinator);
    assert(!incomplete_manager.required_provider_ids());

    CapabilityRegistry slow_registry;
    slow_registry.register_instance(
        descriptor("episode_boundary", "slow-provider"),
        std::make_shared<int>(4));
    auto slow = std::make_shared<StatePort>("slow-provider", "state");
    slow->delay_capture(std::chrono::milliseconds(10));
    register_state_port(slow_registry, slow, "slow-state-port");
    CognitiveCoordinator slow_coordinator(slow_registry, config);
    CognitiveStateManager slow_manager(
        slow_registry, slow_coordinator, std::chrono::milliseconds(1));
    assert(!slow_manager.capture_bundle("slow-capture"));
}
