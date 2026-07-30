#include "core/capability_runtime.hpp"
#include "core/pattern_learner.hpp"

#include <cassert>

using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::ModuleLifecycleManager;
using eu_digital::PatternConfig;
using eu_digital::PatternLearningPlugin;
using eu_digital::PatternLearner;
using eu_digital::PatternObservation;

PatternObservation observation(double x, const char* reference, double timestamp) {
    return PatternObservation{{{"x", x}}, reference, timestamp};
}

int main() {
    PatternLearner learner(PatternConfig{2, 0.1, 0.5}, "stream-test");
    const auto first = learner.observe(observation(0.0, "obs-1", 0.0));
    const auto second = learner.observe(observation(0.0, "obs-2", 1.0));
    assert(first.status == "candidate");
    assert(second.status == "promoted");
    assert(second.observation_refs.size() == 2);
    const auto corrected = learner.feedback(second.pattern_id, false, "human-1");
    assert(corrected.status == "candidate");
    assert(corrected.negative_feedback == 1);

    const auto drifted = learner.observe(observation(1.0, "obs-3", 2.0));
    assert(drifted.version == 2);
    assert(drifted.parent_pattern_id == first.pattern_id);
    assert(drifted.drift_reason == "concept_drift");

    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    PatternLearningPlugin plugin;
    assert(lifecycle.install(plugin));
    assert(registry.record("native.pattern_learner").state.state == CapabilityState::available);
    lifecycle.remove("native.pattern_learner");
    assert(registry.record("native.pattern_learner").state.state == CapabilityState::removed);
}
