#include "core/policies/cognitive_decision_policy.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

void test_direct_request_bypasses_budget() {
    auto orchestrator = std::make_shared<SuggestionOrchestrator>();
    // Set budget to 0 to simulate exhausted budget
    SuggestionPolicy policy;
    policy.max_per_day = 0;
    policy.max_per_window = 0;
    orchestrator = std::make_shared<SuggestionOrchestrator>(policy);

    CognitiveDecisionPolicy decision_policy(orchestrator);

    CanonicalEvent event;
    event.event_type = "user_explicit_question";
    event.event_id = "e1";

    auto req = decision_policy.decide(event, "2026-07-31T12:00:00Z");

    // Even if budget is 0, explicit question should bypass and give requested_response
    assert(req.intent == "requested_response");
    std::cout << "test_direct_request_bypasses_budget passed\n";
}

void test_proactive_suggestion_evaluates_budget() {
    auto orchestrator = std::make_shared<SuggestionOrchestrator>();
    CognitiveDecisionPolicy decision_policy(orchestrator);

    CanonicalEvent event;
    event.event_type = "system_observation";
    event.event_id = "e2";

    auto req = decision_policy.decide(event, "2026-07-31T12:00:00Z");

    assert(req.intent == "proactive_suggestion");

    // Exhaust budget
    SuggestionPolicy policy;
    policy.max_per_day = 0;
    policy.max_per_window = 0;
    auto orchestrator_exhausted = std::make_shared<SuggestionOrchestrator>(policy);
    CognitiveDecisionPolicy decision_policy_exhausted(orchestrator_exhausted);

    auto req_exhausted = decision_policy_exhausted.decide(event, "2026-07-31T12:00:00Z");
    assert(req_exhausted.intent == "silence");

    std::cout << "test_proactive_suggestion_evaluates_budget passed\n";
}

int main() {
    test_direct_request_bypasses_budget();
    test_proactive_suggestion_evaluates_budget();
    return 0;
}
