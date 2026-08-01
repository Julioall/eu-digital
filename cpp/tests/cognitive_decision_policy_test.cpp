#include "core/policies/cognitive_decision_policy.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

void test_direct_request_bypasses_budget() {
    SuggestionPolicy policy;
    policy.max_per_day = 1;
    policy.max_per_window = 1;
    auto orchestrator = std::make_shared<SuggestionOrchestrator>(policy);

    CognitiveDecisionPolicy decision_policy(orchestrator);

    // Consume the single budget allowance to simulate exhausted budget
    CanonicalEvent dummy_event;
    dummy_event.event_type = "system_observation";
    dummy_event.event_id = "dummy";
    decision_policy.decide(dummy_event, "2026-07-31T11:00:00Z");

    // Now budget is exhausted. An explicit request should STILL bypass it.
    CanonicalEvent event;
    event.event_type = "user_explicit_question";
    event.event_id = "e1";

    auto req = decision_policy.decide(event, "2026-07-31T12:00:00Z");

    assert(req.intent == "requested_response");
    std::cout << "test_direct_request_bypasses_budget passed\n";
}

void test_proactive_suggestion_evaluates_budget() {
    SuggestionPolicy policy;
    policy.max_per_day = 1;
    policy.max_per_window = 1;
    auto orchestrator = std::make_shared<SuggestionOrchestrator>(policy);
    CognitiveDecisionPolicy decision_policy(orchestrator);

    CanonicalEvent event;
    event.event_type = "system_observation";
    event.event_id = "e2";

    // First call consumes the budget
    auto req = decision_policy.decide(event, "2026-07-31T12:00:00Z");
    assert(req.intent == "proactive_suggestion");

    // Second call is exhausted
    CanonicalEvent event3;
    event3.event_type = "system_observation";
    event3.event_id = "e3";
    auto req_exhausted = decision_policy.decide(event3, "2026-07-31T12:05:00Z");
    assert(req_exhausted.intent == "silence");

    std::cout << "test_proactive_suggestion_evaluates_budget passed\n";
}

int main() {
    test_direct_request_bypasses_budget();
    test_proactive_suggestion_evaluates_budget();
    return 0;
}
