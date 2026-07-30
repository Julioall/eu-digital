#include "core/capability_runtime.hpp"
#include "core/procedural_avatar.hpp"

#include <cassert>
#include <stdexcept>

using eu_digital::AvatarFeedbackAction;
using eu_digital::AvatarHealthStatus;
using eu_digital::AvatarPresentationProfile;
using eu_digital::AvatarRenderControls;
using eu_digital::AvatarRenderQuota;
using eu_digital::AvatarViewKind;
using eu_digital::AvatarViewState;
using eu_digital::CapabilityPlugin;
using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::ModuleLifecycleManager;
using eu_digital::NoCapabilityProvider;
using eu_digital::ProceduralAvatarPlugin;
using eu_digital::ProceduralAvatarRenderer;

namespace {

AvatarViewState question() {
    return {"avatar-test", AvatarViewKind::question, std::string("notice-1")};
}

class FailingAvatarPlugin final : public CapabilityPlugin {
public:
    FailingAvatarPlugin() {
        descriptor_.capability_id = "presentation.procedural_avatar";
        descriptor_.implementation_id = "test.failing_avatar";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "presentation";
        descriptor_.provides.push_back({"render.avatar_frame", "urn:eu-digital:avatar-frame:1"});
    }

    const eu_digital::CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return false; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    eu_digital::CapabilityDescriptor descriptor_;
};

class SubstituteAvatarPlugin final : public CapabilityPlugin {
public:
    SubstituteAvatarPlugin() {
        descriptor_.capability_id = "presentation.procedural_avatar";
        descriptor_.implementation_id = "test.substitute_avatar";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "presentation";
        descriptor_.provides.push_back({"render.avatar_frame", "urn:eu-digital:avatar-frame:1"});
        descriptor_.supports_hot_plug = true;
    }

    const eu_digital::CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    eu_digital::CapabilityDescriptor descriptor_;
};

}  // namespace

int main() {
    AvatarPresentationProfile profile;
    ProceduralAvatarRenderer without_consent(profile, question());
    const auto denied = without_consent.render(32, 32, 0);
    assert(!denied.rendered);
    assert(denied.reason == "consent_required");
    assert(without_consent.health().status == AvatarHealthStatus::unavailable);

    AvatarRenderControls controls;
    controls.consent_granted = true;
    AvatarRenderQuota quota{2, 0};
    ProceduralAvatarRenderer renderer(profile, question(), controls, quota);
    const auto first = renderer.render(32, 32, 7);
    const auto second = renderer.render(32, 32, 7);
    assert(first.rendered);
    assert(first.rgba == second.rgba);
    assert(renderer.quota().consumed_frames == 2);
    const auto exhausted = renderer.render(32, 32, 8);
    assert(!exhausted.rendered);
    assert(exhausted.reason == "quota_exhausted");
    renderer.begin_quota_window();
    renderer.set_global_pause(true);
    assert(renderer.render(32, 32, 9).reason == "globally_paused");
    renderer.set_global_pause(false);
    renderer.apply_feedback(AvatarFeedbackAction::correct, "not a confirmed pattern");
    assert(renderer.view().state == AvatarViewKind::quiet);
    renderer.set_view(question());
    renderer.apply_feedback(AvatarFeedbackAction::silence);
    assert(renderer.view().state == AvatarViewKind::hidden);
    assert(renderer.history().size() == 2);

    for (const auto shape : {eu_digital::AvatarShape::particles, eu_digital::AvatarShape::filament,
                             eu_digital::AvatarShape::smoke, eu_digital::AvatarShape::metaball}) {
        auto shaped_profile = profile;
        shaped_profile.shape = shape;
        ProceduralAvatarRenderer shaped(shaped_profile, question(), controls, AvatarRenderQuota{1, 0});
        const auto frame = shaped.render(24, 24, 3);
        assert(frame.rendered);
        assert(frame.rgba.size() == 24 * 24);
    }

    bool invalid_profile = false;
    try {
        auto invalid = profile;
        invalid.density = 1.1;
        invalid.validate();
    } catch (const std::invalid_argument&) {
        invalid_profile = true;
    }
    assert(invalid_profile);

    bool invalid_view = false;
    try {
        auto invalid = question();
        invalid.has_focus = true;
        invalid.validate();
    } catch (const std::invalid_argument&) {
        invalid_view = true;
    }
    assert(invalid_view);

    bool invalid_override = false;
    try {
        auto invalid = profile;
        invalid.override_audit.active = true;
        invalid.validate();
    } catch (const std::invalid_argument&) {
        invalid_override = true;
    }
    assert(invalid_override);

    CapabilityRegistry registry;
    bool absent = false;
    try {
        (void)registry.resolve("render.avatar_frame");
    } catch (const NoCapabilityProvider&) {
        absent = true;
    }
    assert(absent);

    ModuleLifecycleManager lifecycle(registry);
    FailingAvatarPlugin failing;
    assert(!lifecycle.install(failing));
    assert(registry.record("test.failing_avatar").state.state == CapabilityState::failed);

    ProceduralAvatarPlugin plugin;
    assert(lifecycle.install(plugin));
    assert(registry.record("native.procedural_avatar").state.state == CapabilityState::available);
    assert(registry.resolve("render.avatar_frame").implementation_id == "native.procedural_avatar");
    lifecycle.remove("native.procedural_avatar");
    assert(registry.record("native.procedural_avatar").state.state == CapabilityState::removed);
    assert(lifecycle.install(plugin));

    SubstituteAvatarPlugin substitute;
    assert(lifecycle.install(substitute, 10));
    assert(registry.resolve("render.avatar_frame").implementation_id == "test.substitute_avatar");
    assert(registry.resolve("render.avatar_frame", "native.procedural_avatar").implementation_id == "native.procedural_avatar");
    lifecycle.remove("test.substitute_avatar");
    lifecycle.remove("native.procedural_avatar");
    return 0;
}
