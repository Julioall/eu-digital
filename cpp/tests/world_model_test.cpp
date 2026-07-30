#include "core/capability_runtime.hpp"
#include "core/world_model.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::ModuleLifecycleManager;
using eu_digital::WorldModel;
using eu_digital::WorldModelConfig;
using eu_digital::WorldModelPlugin;
using eu_digital::WorldModelPolicy;

void seed(WorldModel& model, const std::vector<std::string>& states) {
    for (std::size_t index = 0; index < states.size(); ++index) {
        model.observe(states[index], "event-" + std::to_string(index), static_cast<double>(index));
    }
}

int main() {
    WorldModel model(WorldModelConfig{}, "stream-test");
    seed(model, {"idle", "active", "idle"});
    const auto prediction = model.predict({"idle"}, "2026-01-01T00:01:00Z", {"idle", "active"});
    assert(prediction.predicted_distribution.size() == 2);
    double total = 0.0;
    for (const auto& [unused, probability] : prediction.predicted_distribution) total += probability;
    assert(std::abs(total - 1.0) < 1e-12);
    assert(!prediction.log_loss.has_value());
    const auto scored = model.score(prediction, "active", "2026-01-01T00:01:01Z");
    assert(scored.log_loss.has_value());
    assert(scored.top_k_hit.has_value());
    assert(scored.salience_contribution > 0.0);

    WorldModel treatment(WorldModelConfig{}, "treatment", WorldModelPolicy::incremental);
    WorldModel baseline(WorldModelConfig{}, "baseline", WorldModelPolicy::frequency);
    seed(treatment, {"a", "b", "a", "b"});
    seed(baseline, {"a", "b", "a", "b"});
    double treatment_loss = 0.0;
    double baseline_loss = 0.0;
    for (int index = 0; index < 2; ++index) {
        const std::string expected = index == 0 ? "a" : "b";
        const auto treatment_score = treatment.score(
            treatment.predict({index == 0 ? "a" : "b", index == 0 ? "b" : "a"}, "2026-01-01T00:02:00Z", {"a", "b"}),
            expected, "2026-01-01T00:02:01Z");
        const auto baseline_score = baseline.score(
            baseline.predict({index == 0 ? "a" : "b", index == 0 ? "b" : "a"}, "2026-01-01T00:02:00Z", {"a", "b"}),
            expected, "2026-01-01T00:02:01Z");
        treatment_loss += *treatment_score.log_loss;
        baseline_loss += *baseline_score.log_loss;
        treatment.observe(expected, "holdout-treatment-" + std::to_string(index), 10.0 + index);
        baseline.observe(expected, "holdout-baseline-" + std::to_string(index), 10.0 + index);
    }
    assert(treatment_loss < baseline_loss);

    WorldModelConfig drift_config;
    drift_config.drift_window = 2;
    drift_config.drift_threshold = 0.1;
    WorldModel drift_model(drift_config, "drift-test");
    seed(drift_model, {"a", "a", "a"});
    for (int index = 0; index < 2; ++index) {
        const auto scored_drift = drift_model.score(
            drift_model.predict({"a"}, "2026-01-01T00:03:00Z", {"a", "b"}),
            "b", "2026-01-01T00:03:01Z");
        if (index == 0) assert(!scored_drift.drift_id.has_value());
    }
    assert(drift_model.drifts().size() == 1);
    assert(drift_model.drifts().front().confidence_after < drift_model.drifts().front().confidence_before);
    drift_model.observe("b", "relearn-1", 20.0);
    assert(drift_model.relearning_observations() == 1);

    WorldModel candidate_only(
        WorldModelConfig{}, "candidate-only", WorldModelPolicy::incremental,
        {{"a", "promoted", 0.9}, {"b", "promoted", 0.8}});
    const auto uncertain = candidate_only.predict({}, "2026-01-01T00:04:00Z");
    assert(std::abs(uncertain.predicted_distribution.at("a") - 0.5) < 1e-12);
    assert(std::abs(uncertain.predicted_distribution.at("b") - 0.5) < 1e-12);
    assert(candidate_only.promoted_pattern_count() == 2);

    bool invalid_pattern_rejected = false;
    try {
        WorldModel invalid_pattern(
            WorldModelConfig{}, "invalid-pattern", WorldModelPolicy::incremental,
            {{"candidate", "candidate", 0.5}});
    } catch (const std::invalid_argument&) {
        invalid_pattern_rejected = true;
    }
    assert(invalid_pattern_rejected);

    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    WorldModelPlugin plugin;
    assert(lifecycle.install(plugin));
    assert(registry.record("native.world_model").state.state == CapabilityState::available);
    lifecycle.remove("native.world_model");
    assert(registry.record("native.world_model").state.state == CapabilityState::removed);
}
