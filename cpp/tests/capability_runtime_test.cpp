#include "core/capability_runtime.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <vector>

using eu_digital::CapabilityDescriptor;
using eu_digital::CapabilityLifecycleError;
using eu_digital::CapabilityOperation;
using eu_digital::CapabilityPlugin;
using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::ModuleLifecycleManager;
using eu_digital::NoCapabilityProvider;

class FakePlugin final : public CapabilityPlugin {
public:
    explicit FakePlugin(std::string id, bool healthy = true) : healthy_(healthy) {
        descriptor_.capability_id = "test.sensor";
        descriptor_.implementation_id = std::move(id);
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "sensor";
        descriptor_.provides.push_back({"observe.test", "urn:test:observation"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = true;
    }

    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override { calls.push_back("validate_manifest"); }
    void configure() override { calls.push_back("configure"); }
    void initialize() override { calls.push_back("initialize"); }
    void calibrate() override { calls.push_back("calibrate"); }
    bool health_check() override { calls.push_back("health_check"); return healthy_; }
    void start() override { calls.push_back("start"); }
    void drain() override { calls.push_back("drain"); }
    std::map<std::string, std::string> checkpoint() override { calls.push_back("checkpoint"); return {{"counter", "1"}}; }
    void stop() override { calls.push_back("stop"); }
    void uninstall() override { calls.push_back("uninstall"); }

    std::vector<std::string> calls;

private:
    CapabilityDescriptor descriptor_;
    bool healthy_;
};

int main() {
    std::vector<std::string> events;
    CapabilityRegistry registry([&](const std::string& type, const std::string&) { events.push_back(type); });
    ModuleLifecycleManager lifecycle(registry);
    FakePlugin primary("primary");
    FakePlugin fallback("fallback");

    assert(registry.records().empty());
    registry.discover(primary.descriptor(), 1);
    lifecycle.attach("primary", primary);
    lifecycle.attach("fallback", fallback);
    assert(lifecycle.install(fallback, 10));
    assert(registry.record("fallback").state.state == CapabilityState::available);

    registry.register_plan("plan-1", {"fallback"});
    registry.define_profile("fallback-only", {"fallback"});
    registry.activate_profile("fallback-only");
    registry.save("capability_runtime_test.snapshot");
    auto restored = CapabilityRegistry::load("capability_runtime_test.snapshot");
    assert(restored.record("fallback").state.state == CapabilityState::available);
    assert(restored.active_profile() == "fallback-only");
    assert(restored.resolve("observe.test").implementation_id == "fallback");

    registry.activate_profile(std::nullopt);
    registry.register_plan("plan-2", {"primary"});
    const auto resolution = registry.resolve("observe.test", "primary");
    assert(resolution.implementation_id == "fallback");
    assert(resolution.fallback);

    lifecycle.remove("fallback");
    assert(registry.record("fallback").state.state == CapabilityState::removed);
    assert(!events.empty());

    try {
        registry.resolve("missing.operation");
        assert(false);
    } catch (const NoCapabilityProvider&) {
    }

    FakePlugin broken("broken", false);
    lifecycle.attach("broken", broken);
    assert(!lifecycle.install(broken));
    assert(registry.record("broken").state.state == CapabilityState::failed);

    // Stateful providers are enumerated deterministically so a checkpoint never
    // depends on discovery order. Removed providers disappear and may be
    // reinstalled or substituted without becoming structural dependencies.
    CapabilityDescriptor low_descriptor;
    low_descriptor.capability_id = "test.state";
    low_descriptor.implementation_id = "state-low";
    low_descriptor.implementation_version = "1.0.0";
    low_descriptor.kind = "state_provider";
    low_descriptor.provides.push_back({"cognitive_state", "urn:test:state"});
    low_descriptor.supports_checkpoint = true;
    registry.register_instance(low_descriptor, std::make_shared<int>(1), 1);

    auto high_a = low_descriptor;
    high_a.implementation_id = "state-a";
    auto high_z = low_descriptor;
    high_z.implementation_id = "state-z";
    registry.register_instance(high_z, std::make_shared<int>(3), 10);
    registry.register_instance(high_a, std::make_shared<int>(2), 10);

    auto state_providers = registry.resolve_all<int>("cognitive_state");
    assert(state_providers.size() == 3);
    assert(*state_providers[0] == 2);
    assert(*state_providers[1] == 3);
    assert(*state_providers[2] == 1);

    registry.transition("state-a", CapabilityState::removed, "test_removal");
    state_providers = registry.resolve_all<int>("cognitive_state");
    assert(state_providers.size() == 2);
    assert(*state_providers[0] == 3);

    registry.register_instance(high_a, std::make_shared<int>(4), 10);
    state_providers = registry.resolve_all<int>("cognitive_state");
    assert(state_providers.size() == 3);
    assert(*state_providers[0] == 4);

    registry.transition("state-z", CapabilityState::removed, "test_substitution");
    auto substitute = low_descriptor;
    substitute.implementation_id = "state-substitute";
    registry.register_instance(substitute, std::make_shared<int>(5), 20);
    state_providers = registry.resolve_all<int>("cognitive_state");
    assert(state_providers.size() == 3);
    assert(*state_providers[0] == 5);

    assert(registry.resolve_all<int>("missing.state").empty());
}
