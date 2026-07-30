#include "core/metacognition_curiosity.hpp"

#include <cassert>
#include <string>

namespace {

eu_digital::HypothesisRecord make_hypothesis(const std::string& id, double confidence = 0.5) {
    return {
        id,
        "predictive",
        "hypothesis " + id,
        eu_digital::HypothesisStatus::proposed,
        confidence,
        {"support-1"},
        {"oppose-1"},
        {"alternative-a"},
        "2026-07-28T12:00:00+00:00",
        "2026-07-28T12:00:00+00:00",
        std::nullopt,
        std::nullopt,
        "test",
        std::nullopt,
        "1.0"};
}

}  // namespace

int main() {
    using namespace eu_digital;

    MetacognitionCuriosityEngine engine(CuriosityConfig{true, QuestionPolicy::information_gain_v1, 3, 900.0, 0.0, 1800.0, 0.1, 1.0, true, true, true, 10});
    const auto assessment = engine.evaluate(make_hypothesis("h-test"), "2026-07-28T12:00:00+00:00");
    assert(assessment.decision == "question");
    assert(assessment.supporting_refs.size() == 1);
    const auto proposed = engine.propose_question(
        assessment.assessment_id, "Which observation distinguishes the alternatives?", 0.8,
        "2026-07-28T12:00:00+00:00");
    assert(proposed.status == QuestionStatus::proposed);
    assert(proposed.assessment_id == assessment.assessment_id);
    const auto asked = engine.ask(proposed.question_id, "2026-07-28T12:00:00+00:00");
    assert(asked.status == QuestionStatus::asked);
    const auto response = engine.record_response(
        proposed.question_id, ResponseOutcome::inconclusive, false, {"missing-observation"},
        "fixture", std::nullopt, "2026-07-28T12:00:01+00:00");
    assert(response.outcome == ResponseOutcome::inconclusive);
    assert(engine.metrics_json().find("\"outcome_count\":0") != std::string::npos);
    assert(engine.snapshot_json().find("\"schema_version\":\"1.0\"") != std::string::npos);

    CuriosityConfig baseline_config;
    baseline_config.calibration_enabled = false;
    baseline_config.question_policy = QuestionPolicy::fixed_gain_v0;
    baseline_config.budget_enabled = false;
    baseline_config.cooldown_enabled = false;
    baseline_config.redundancy_suppression_enabled = false;
    baseline_config.silence_confidence = 1.0;
    MetacognitionCuriosityEngine baseline(baseline_config);
    const auto baseline_assessment = baseline.evaluate(make_hypothesis("h-baseline", 0.7), "2026-07-28T12:00:00+00:00");
    const auto baseline_question = baseline.propose_question(
        baseline_assessment.assessment_id, "Fixed question?", 0.1, "2026-07-28T12:00:00+00:00");
    assert(baseline_assessment.calibrated_confidence == baseline_assessment.raw_confidence);
    assert(baseline_question.expected_information_gain == 0.5);

    MetacognitionCuriosityPlugin plugin;
    assert(plugin.descriptor().capability_id == "cognition.metacognition_curiosity");
    assert(plugin.descriptor().supports_hot_plug);

    bool rejected = false;
    try {
        engine.ask("missing", "2026-07-28T12:00:00+00:00");
    } catch (const MetacognitionCuriosityError&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
