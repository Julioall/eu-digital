#include "core/supervised_actions.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using eu_digital::ActionAuthorization;
using eu_digital::ActionOutcomeStatus;
using eu_digital::ActionPlan;
using eu_digital::ActionPolicy;
using eu_digital::ActionPolicyDecision;
using eu_digital::ActionPort;
using eu_digital::ActionPortResult;
using eu_digital::ActionSimulation;
using eu_digital::CanonicalEvent;
using eu_digital::SupervisedActionController;

namespace {

ActionPlan plan(const char* digest = "sha256:plan-1") {
    return {"plan-1", "create_note", "local://notes/1", digest, {"create local note"}, false};
}

ActionAuthorization authorization(const char* digest = "sha256:plan-1", std::uint64_t expiry = 2000) {
    return {"auth-1", "plan-1", digest, "user-1", 1000, expiry, "explicit"};
}

class FakePolicy final : public ActionPolicy {
public:
    bool allowed{true};
    int calls{};

    ActionPolicyDecision evaluate(const ActionPlan&, const ActionSimulation&) override {
        ++calls;
        return {allowed, "fixture-policy-v1", allowed ? "allowed_for_test" : "policy_denied"};
    }
};

class FakePort final : public ActionPort {
public:
    int simulate_calls{};
    int execute_calls{};
    int rollback_calls{};
    bool execute_success{true};
    bool rollback_success{true};
    bool reversible{true};

    ActionSimulation simulate(const ActionPlan& value) override {
        ++simulate_calls;
        return {value.plan_id, value.plan_digest, {"create local note"}, "low risk", reversible, 0.25};
    }

    ActionPortResult execute(const ActionPlan&) override {
        ++execute_calls;
        if (!execute_success) return {false, {}, "fixture_execution_failed"};
        return {true, {"note created"}, {}};
    }

    bool rollback(const ActionPlan&) override {
        ++rollback_calls;
        return rollback_success;
    }
};

}  // namespace

int main() {
    FakePort port;
    FakePolicy policy;
    std::vector<CanonicalEvent> events;
    SupervisedActionController controller(&port, policy, [&](const CanonicalEvent& event) {
        events.push_back(event);
    });

    assert(controller.descriptor().valid());
    assert(controller.descriptor().kind == "actuator");
    const auto preparation = controller.prepare(plan(), 900);
    assert(preparation.ready_for_confirmation);
    assert(preparation.confirmation_required);
    assert(preparation.simulation.effects.size() == 1);
    assert(port.simulate_calls == 1);
    assert(port.execute_calls == 0);

    auto outcome = controller.execute("plan-1", 950);
    assert(outcome.status == ActionOutcomeStatus::blocked);
    assert(outcome.error_code == "confirmation_required");
    assert(port.execute_calls == 0);

    assert(!controller.authorize("plan-1", authorization("sha256:changed"), 1100));
    assert(!controller.authorize("plan-1", authorization("sha256:plan-1", 1000), 1101));
    assert(controller.authorize("plan-1", authorization(), 1100));
    outcome = controller.execute("plan-1", 1200);
    assert(outcome.status == ActionOutcomeStatus::succeeded);
    assert(outcome.authorized_by == "user-1");
    assert(port.execute_calls == 1);
    assert(!events.empty());
    assert(events.back().event_type == "action.audit");
    assert(controller.execute("plan-1", 1300).status == ActionOutcomeStatus::blocked);
    assert(port.execute_calls == 1);

    outcome = controller.rollback("plan-1", 1400);
    assert(outcome.status == ActionOutcomeStatus::rolled_back);
    assert(port.rollback_calls == 1);

    policy.allowed = false;
    const auto denied = controller.prepare(plan("sha256:plan-2"), 1500);
    assert(!denied.ready_for_confirmation);
    assert(!controller.authorize("plan-1", authorization(), 1600));

    FakePort failing_port;
    failing_port.execute_success = false;
    FakePolicy failing_policy;
    SupervisedActionController failing(&failing_port, failing_policy, {});
    assert(failing.prepare(plan(), 900).ready_for_confirmation);
    assert(failing.authorize("plan-1", authorization(), 1100));
    assert(failing.execute("plan-1", 1200).status == ActionOutcomeStatus::failed);
    assert(failing.rollback("plan-1", 1300).status == ActionOutcomeStatus::blocked);

    FakePolicy absent_policy;
    SupervisedActionController absent(nullptr, absent_policy, {});
    assert(!absent.prepare(plan(), 900).ready_for_confirmation);
    assert(absent.execute("plan-1", 1000).status == ActionOutcomeStatus::blocked);
    assert(absent_policy.calls == 0);

    return 0;
}
