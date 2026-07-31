#include "core/suggestion_orchestrator.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

static int passed = 0;
static int total = 0;

static void check(bool condition, const char* label) {
    ++total;
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        throw std::runtime_error(label);
    }
    ++passed;
}

static void expect_throw(auto callable, const char* label) {
    ++total;
    try {
        callable();
        std::cerr << "FAIL (no throw): " << label << '\n';
        throw std::runtime_error(label);
    } catch (const eu_digital::SuggestionOrchestratorError&) {
        ++passed;
    }
}

static eu_digital::SuggestionEvidence make_evidence(
    const std::string& hypothesis_id, double confidence, double gain,
    const std::string& reason = "pattern observed") {
    return {hypothesis_id, confidence, gain, {"ev-" + hypothesis_id + "-1"}, reason};
}

static void test_basic_suggestion() {
    eu_digital::SuggestionOrchestrator orch;
    auto evidence = make_evidence("h-001", 0.6, 0.3);
    auto decision = orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");

    check(!decision.decision_id.empty(), "decision has id");
    check(decision.schema_version == "1.0", "schema version");
    check(decision.hypothesis_id == "h-001", "hypothesis id");
    check(std::abs(decision.confidence - 0.6) < 1e-9, "confidence");
    check(std::abs(decision.information_gain - 0.3) < 1e-9, "information gain");
    check(!decision.suppressed, "not suppressed");
    check(!decision.suppression_reason.has_value(), "no suppression reason");
    check(!decision.action_proposed, "no action proposed");
    check(decision.budget_before == 3, "budget starts at 3");
    check(decision.budget_after == 2, "budget after delivery is 2");
    check(!decision.created_at.empty(), "created_at present");
}

static void test_action_proposed_always_false() {
    eu_digital::SuggestionDecision decision{};
    decision.decision_id = "test-1";
    decision.policy_id = "p";
    decision.policy_version = "1";
    decision.evidence_ids = {"e-1"};
    decision.hypothesis_id = "h-1";
    decision.confidence = 0.5;
    decision.information_gain = 0.1;
    decision.reason = "test";
    decision.created_at = "2026-07-31T12:00:00+00:00";
    decision.action_proposed = true;
    expect_throw([&] { decision.validate(); }, "action_proposed=true is rejected");
}

static void test_budget_exhaustion() {
    eu_digital::SuggestionPolicy policy;
    policy.max_per_window = 2;
    policy.window_seconds = 900.0;
    policy.redundancy_suppression = false;  // allow same hypothesis
    eu_digital::SuggestionOrchestrator orch(policy);

    auto e1 = make_evidence("h-001", 0.6, 0.3);
    auto e2 = make_evidence("h-002", 0.7, 0.4);
    auto e3 = make_evidence("h-003", 0.8, 0.5);

    auto d1 = orch.evaluate(e1, "2026-07-31T12:00:00+00:00");
    check(!d1.suppressed, "first not suppressed");
    check(d1.budget_before == 2, "budget starts at 2");

    auto d2 = orch.evaluate(e2, "2026-07-31T12:01:00+00:00");
    check(!d2.suppressed, "second not suppressed");
    check(d2.budget_before == 1, "budget was 1");

    auto d3 = orch.evaluate(e3, "2026-07-31T12:02:00+00:00");
    check(d3.suppressed, "third is suppressed");
    check(d3.suppression_reason.value() == "budget_exhausted", "suppression reason is budget");
    check(d3.budget_before == 0, "budget was 0");
    check(d3.budget_after == 0, "budget stays 0");
}

static void test_cooldown() {
    eu_digital::SuggestionPolicy policy;
    policy.cooldown_seconds = 300.0;
    eu_digital::SuggestionOrchestrator orch(policy);

    auto evidence = make_evidence("h-001", 0.6, 0.3);
    auto d1 = orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");
    check(!d1.suppressed, "first not suppressed");

    // Same hypothesis within cooldown
    auto d2 = orch.evaluate(evidence, "2026-07-31T12:03:00+00:00");
    check(d2.suppressed, "cooldown suppressed");
    check(d2.suppression_reason.value() == "cooldown_active", "cooldown reason");
    check(d2.cooldown_remaining_seconds > 0.0, "cooldown remaining > 0");

    // After cooldown expires (but redundancy suppression kicks in)
    auto d3 = orch.evaluate(evidence, "2026-07-31T12:06:00+00:00");
    check(d3.suppressed, "redundancy suppressed after cooldown");
    check(d3.suppression_reason.value() == "redundant_hypothesis", "redundancy reason");
}

static void test_correction_resets_redundancy() {
    eu_digital::SuggestionPolicy policy;
    policy.cooldown_seconds = 60.0;
    policy.correction_cooldown_seconds = 120.0;
    eu_digital::SuggestionOrchestrator orch(policy);

    auto evidence = make_evidence("h-001", 0.6, 0.3);
    auto d1 = orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");
    check(!d1.suppressed, "first delivered");

    // Record correction on d1
    orch.record_feedback(d1.decision_id, eu_digital::SuggestionFeedback::correct,
                         std::string("wrong context"), "2026-07-31T12:00:30+00:00");

    // After correction cooldown (120s), same hypothesis should not be redundant
    auto d2 = orch.evaluate(evidence, "2026-07-31T12:03:00+00:00");
    check(!d2.suppressed, "after correction, hypothesis is not redundant");
}

static void test_confidence_below_minimum() {
    eu_digital::SuggestionOrchestrator orch;
    auto evidence = make_evidence("h-001", 0.05, 0.3);  // below min_confidence 0.15
    auto d = orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");
    check(d.suppressed, "low confidence suppressed");
    check(d.suppression_reason.value() == "confidence_below_minimum", "confidence reason");
}

static void test_gain_below_minimum() {
    eu_digital::SuggestionOrchestrator orch;
    auto evidence = make_evidence("h-001", 0.6, 0.01);  // below min_gain 0.05
    auto d = orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");
    check(d.suppressed, "low gain suppressed");
    check(d.suppression_reason.value() == "information_gain_below_minimum", "gain reason");
}

static void test_model_absent_degradation() {
    eu_digital::SuggestionOrchestrator orch({}, false);
    auto evidence = make_evidence("h-001", 0.6, 0.3);
    auto d = orch.evaluate_without_model(evidence, "2026-07-31T12:00:00+00:00");
    check(d.suppressed, "model absent is suppressed");
    check(d.suppression_reason.value() == "model_absent", "model absent reason");
    check(d.reason.find("model absent") != std::string::npos, "reason mentions model absent");
    check(!d.action_proposed, "no action proposed");
}

static void test_model_absent_rejects_normal_evaluate() {
    eu_digital::SuggestionOrchestrator orch({}, true);
    auto evidence = make_evidence("h-001", 0.6, 0.3);
    expect_throw([&] {
        orch.evaluate_without_model(evidence, "2026-07-31T12:00:00+00:00");
    }, "model available but called evaluate_without_model");
}

static void test_daily_budget() {
    eu_digital::SuggestionPolicy policy;
    policy.max_per_window = 100;  // high window budget
    policy.max_per_day = 2;       // low daily budget
    policy.redundancy_suppression = false;
    policy.cooldown_enabled = false;
    eu_digital::SuggestionOrchestrator orch(policy);

    auto d1 = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T08:00:00+00:00");
    check(!d1.suppressed, "first within daily");
    auto d2 = orch.evaluate(make_evidence("h-002", 0.6, 0.3), "2026-07-31T09:00:00+00:00");
    check(!d2.suppressed, "second within daily");
    auto d3 = orch.evaluate(make_evidence("h-003", 0.6, 0.3), "2026-07-31T10:00:00+00:00");
    check(d3.suppressed, "third exceeds daily budget");
    check(d3.suppression_reason.value() == "budget_exhausted", "daily budget reason");
}

static void test_feedback_on_suppressed_rejects() {
    eu_digital::SuggestionPolicy policy;
    policy.max_per_window = 1;
    policy.redundancy_suppression = false;
    eu_digital::SuggestionOrchestrator orch(policy);
    // Exhaust budget
    orch.evaluate(make_evidence("h-000", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    // Next one will be suppressed
    auto d = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:30+00:00");
    check(d.suppressed, "suppressed");
    expect_throw([&] {
        orch.record_feedback(d.decision_id, eu_digital::SuggestionFeedback::defer,
                             std::nullopt, "2026-07-31T12:01:00+00:00");
    }, "feedback on suppressed rejected");
}

static void test_feedback_correct_requires_correction() {
    eu_digital::SuggestionOrchestrator orch;
    auto d = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    expect_throw([&] {
        orch.record_feedback(d.decision_id, eu_digital::SuggestionFeedback::correct,
                             std::nullopt, "2026-07-31T12:01:00+00:00");
    }, "correct without correction text");
}

static void test_feedback_defer_rejects_correction() {
    eu_digital::SuggestionOrchestrator orch;
    auto d = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    expect_throw([&] {
        orch.record_feedback(d.decision_id, eu_digital::SuggestionFeedback::defer,
                             std::string("wrong"), "2026-07-31T12:01:00+00:00");
    }, "defer with correction text");
}

static void test_evidence_required() {
    eu_digital::SuggestionOrchestrator orch;
    eu_digital::SuggestionEvidence evidence{"h-001", 0.6, 0.3, {}, "reason"};
    expect_throw([&] {
        orch.evaluate(evidence, "2026-07-31T12:00:00+00:00");
    }, "empty evidence rejected");
}

static void test_metrics_json() {
    eu_digital::SuggestionOrchestrator orch;
    auto d1 = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    orch.record_feedback(d1.decision_id, eu_digital::SuggestionFeedback::defer,
                         std::nullopt, "2026-07-31T12:01:00+00:00");
    auto metrics = orch.metrics_json();
    check(metrics.find("\"delivered\":1") != std::string::npos, "delivered count");
    check(metrics.find("\"total_decisions\":1") != std::string::npos, "total decisions");
    check(metrics.find("\"total_feedback\":1") != std::string::npos, "total feedback");
    check(metrics.find("\"hypothesis\"") != std::string::npos, "hypothesis present");
    check(metrics.find("\"falsification\"") != std::string::npos, "falsification present");
    check(metrics.find("\"ablation\"") != std::string::npos, "ablation present");
}

static void test_policy_validation() {
    eu_digital::SuggestionPolicy bad;
    bad.max_per_window = 0;
    expect_throw([&] { bad.validate(); }, "zero max_per_window rejected");

    eu_digital::SuggestionPolicy bad2;
    bad2.min_confidence = 1.5;
    expect_throw([&] { bad2.validate(); }, "min_confidence > 1 rejected");
}

static void test_decision_serialization() {
    eu_digital::SuggestionOrchestrator orch;
    auto d = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    auto json = d.to_json();
    check(json.find("\"action_proposed\":false") != std::string::npos, "json action_proposed false");
    check(json.find("\"hypothesis_id\":\"h-001\"") != std::string::npos, "json hypothesis_id");
    check(json.find("\"schema_version\":\"1.0\"") != std::string::npos, "json schema_version");
    check(json.find("\"suppressed\":false") != std::string::npos, "json not suppressed");
}

static void test_plugin_descriptor() {
    eu_digital::SuggestionOrchestratorPlugin plugin;
    const auto& desc = plugin.descriptor();
    check(desc.capability_id == "cognition.suggestion_orchestrator", "plugin capability id");
    check(desc.kind == "cognition", "plugin kind");
    check(!desc.provides.empty(), "plugin provides something");
    check(plugin.health_check(), "plugin health check");
}

static void test_ablation_no_budget() {
    eu_digital::SuggestionPolicy policy;
    policy.budget_enabled = false;
    policy.cooldown_enabled = false;
    policy.redundancy_suppression = false;
    eu_digital::SuggestionOrchestrator orch(policy);

    // Should deliver unlimited suggestions
    for (int i = 0; i < 10; ++i) {
        auto d = orch.evaluate(
            make_evidence("h-" + std::to_string(i), 0.6, 0.3),
            "2026-07-31T12:0" + std::to_string(i) + ":00+00:00");
        check(!d.suppressed, "ablation: no suppression without budget");
    }
}

static void test_correction_cooldown() {
    eu_digital::SuggestionPolicy policy;
    policy.cooldown_seconds = 60.0;
    policy.correction_cooldown_seconds = 600.0;
    policy.redundancy_suppression = false;
    eu_digital::SuggestionOrchestrator orch(policy);

    auto d1 = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:00:00+00:00");
    orch.record_feedback(d1.decision_id, eu_digital::SuggestionFeedback::correct,
                         std::string("fix"), "2026-07-31T12:00:10+00:00");

    // Within correction cooldown (600s)
    auto d2 = orch.evaluate(make_evidence("h-001", 0.6, 0.3), "2026-07-31T12:05:00+00:00");
    check(d2.suppressed, "within correction cooldown");
    check(d2.suppression_reason.value() == "cooldown_active", "correction cooldown reason");
    check(d2.cooldown_remaining_seconds > 0.0, "cooldown remaining after correction");
}

int main() {
    test_basic_suggestion();
    test_action_proposed_always_false();
    test_budget_exhaustion();
    test_cooldown();
    test_correction_resets_redundancy();
    test_confidence_below_minimum();
    test_gain_below_minimum();
    test_model_absent_degradation();
    test_model_absent_rejects_normal_evaluate();
    test_daily_budget();
    test_feedback_on_suppressed_rejects();
    test_feedback_correct_requires_correction();
    test_feedback_defer_rejects_correction();
    test_evidence_required();
    test_metrics_json();
    test_policy_validation();
    test_decision_serialization();
    test_plugin_descriptor();
    test_ablation_no_budget();
    test_correction_cooldown();

    std::cout << passed << '/' << total << " suggestion orchestrator tests passed\n";
    return passed == total ? 0 : 1;
}
