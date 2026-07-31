#include "core/supervised_actions.hpp"
#include "core/policies/self_model_feedback_policy.hpp"
#include "core/adapters/action_dispatcher_adapter.hpp"
#include "core/contracts/cognitive_decision.hpp"
#include "core/functional_self_model.hpp"
#include "core/event_bus.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using namespace eu_digital;

class TestPolicy final : public ActionPolicy {
public:
    ActionPolicyDecision evaluate(const ActionPlan&, const ActionSimulation&) override {
        return {true, "test", "test_allowed"};
    }
};

class TestPort final : public ActionPort {
public:
    ActionSimulation simulate(const ActionPlan& plan) override {
        return {plan.plan_id, plan.plan_digest, {"effect"}, "low", true, 0.1};
    }
    ActionPortResult execute(const ActionPlan&) override {
        return {true, {"effect"}, ""};
    }
    bool rollback(const ActionPlan&) override {
        return true;
    }
};

int main() {
    TestPort port;
    TestPolicy policy;
    std::vector<CanonicalEvent> emitted_events;
    SupervisedActionPlugin plugin(&port, policy, [&](const CanonicalEvent& event) {
        emitted_events.push_back(event);
    });

    // 1. Test outcome_unknown and ActionOutcome conversion to CanonicalEvent
    ActionPlan plan;
    plan.plan_id = "test-plan-1";
    plan.plan_digest = "test-digest";
    plan.operation = "test-op";
    plan.target = "test-target";
    plan.requested_effects = {"effect"};

    auto prep = plugin.prepare(plan, 100);
    assert(prep.ready_for_confirmation);
    
    // Expire the plan instead of authorizing
    ActionOutcome outcome = plugin.expire(plan.plan_id, 200);
    assert(outcome.status == ActionOutcomeStatus::outcome_unknown);
    
    bool action_outcome_emitted = false;
    for (const auto& ev : emitted_events) {
        if (ev.event_type == "action_outcome") {
            action_outcome_emitted = true;
            // Check that the payload contains outcome_unknown
            assert(ev.payload.find("\"status\":\"outcome_unknown\"") != std::string::npos);
        }
    }
    assert(action_outcome_emitted);

    // 2. Test SelfModelFeedbackPolicy
    VersionedFunctionalSelfModel self_model("test-model", "2026-07-31T00:00:00Z");
    SelfModelFeedbackPolicy feedback_policy(self_model);

    // We can simulate an outcome event
    CanonicalEvent outcome_event;
    outcome_event.event_id = "e1";
    outcome_event.event_type = "action_outcome";
    outcome_event.payload = "{\"status\":\"outcome_unknown\",\"operation\":\"test_action\"}";

    feedback_policy.evaluate(outcome_event, "2026-07-31T01:00:00Z");
    // Initial confidence is 1.0. decay by 0.2 -> 0.8
    assert(self_model.current().capabilities.size() == 1);
    assert(std::abs(self_model.current().capabilities[0].confidence_score - 0.8) < 0.001);

    // Fail again -> 0.6
    CanonicalEvent outcome_event2;
    outcome_event2.event_id = "e2";
    outcome_event2.event_type = "action_outcome";
    outcome_event2.payload = "{\"status\":\"failed\",\"operation\":\"test_action\"}";
    feedback_policy.evaluate(outcome_event2, "2026-07-31T02:00:00Z");
    assert(std::abs(self_model.current().capabilities[0].confidence_score - 0.6) < 0.001);
    assert(self_model.current().capabilities[0].status == "available");

    // Fail again -> 0.4 (degraded)
    CanonicalEvent outcome_event3;
    outcome_event3.event_id = "e3";
    outcome_event3.event_type = "action_outcome";
    outcome_event3.payload = "{\"status\":\"failed\",\"operation\":\"test_action\"}";
    feedback_policy.evaluate(outcome_event3, "2026-07-31T03:00:00Z");
    assert(self_model.current().capabilities[0].confidence_score > 0.39 && self_model.current().capabilities[0].confidence_score < 0.41);
    assert(self_model.current().capabilities[0].status == "degraded");

    // Success -> 0.5 (recovered back to available)
    CanonicalEvent outcome_event4;
    outcome_event4.event_id = "e4";
    outcome_event4.event_type = "action_outcome";
    outcome_event4.payload = "{\"status\":\"succeeded\",\"operation\":\"test_action\"}";
    feedback_policy.evaluate(outcome_event4, "2026-07-31T04:00:00Z");
    assert(std::abs(self_model.current().capabilities[0].confidence_score - 0.5) < 0.001);
    assert(self_model.current().capabilities[0].status == "available");

    // 3. Test ActionDispatcherAdapter idempotency
    ActionDispatcherAdapter dispatcher(plugin);
    
    CognitiveDecision dec = CognitiveDecision::ok("action", "test", "{\"operation\":\"dispatch_test\"}");
    auto dispatch1 = dispatcher.dispatch(dec, 300);
    assert(dispatch1.has_value());
    assert(dispatch1->plan.operation == "dispatch_test");

    auto dispatch2 = dispatcher.dispatch(dec, 400); // Exact same decision payload
    assert(!dispatch2.has_value()); // Blocked by idempotency key

    CognitiveDecision dec2 = CognitiveDecision::ok("action", "test", "{\"operation\":\"dispatch_test\",\"diff\":1}");
    auto dispatch3 = dispatcher.dispatch(dec2, 500); // Different payload -> different digest
    assert(dispatch3.has_value());

    std::cout << "All SPEC-049 tests passed!" << std::endl;
    return 0;
}
