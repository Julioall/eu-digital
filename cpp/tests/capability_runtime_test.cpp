#include "core/capability_runtime.hpp"

#include <cassert>
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
}
