#pragma once

#include "capability_runtime.hpp"
#include "event_bus.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct ActionPlan {
    std::string plan_id;
    std::string operation;
    std::string target;
    std::string plan_digest;
    std::vector<std::string> requested_effects;
    bool destructive{false};

    bool valid() const {
        return !plan_id.empty() && !operation.empty() && !target.empty() && !plan_digest.empty() &&
            !requested_effects.empty();
    }
};

struct ActionSimulation {
    std::string plan_id;
    std::string plan_digest;
    std::vector<std::string> effects;
    std::string risk_summary;
    bool reversible{false};
    double processing_cost_ms{};

    bool valid_for(const ActionPlan& plan) const {
        return plan_id == plan.plan_id && plan_digest == plan.plan_digest && !effects.empty() &&
            !risk_summary.empty() && processing_cost_ms >= 0.0;
    }
};

struct ActionAuthorization {
    std::string authorization_id;
    std::string plan_id;
    std::string plan_digest;
    std::string authorized_by;
    std::uint64_t confirmed_at_ms{};
    std::uint64_t expires_at_ms{};
    std::string confirmation{"explicit"};

    bool valid_at(std::uint64_t now_ms) const {
        return !authorization_id.empty() && !plan_id.empty() && !plan_digest.empty() &&
            !authorized_by.empty() && confirmation == "explicit" && confirmed_at_ms <= now_ms &&
            now_ms <= expires_at_ms;
    }
};

struct ActionPortResult {
    bool succeeded{false};
    std::vector<std::string> effects;
    std::string error_code;
};

struct ActionPolicyDecision {
    bool allowed{false};
    std::string policy_id;
    std::string reason;
};

class ActionPort {
public:
    virtual ~ActionPort() = default;
    virtual ActionSimulation simulate(const ActionPlan& plan) = 0;
    virtual ActionPortResult execute(const ActionPlan& plan) = 0;
    virtual bool rollback(const ActionPlan& plan) = 0;
};

class ActionPolicy {
public:
    virtual ~ActionPolicy() = default;
    virtual ActionPolicyDecision evaluate(const ActionPlan& plan, const ActionSimulation& simulation) = 0;
};

enum class ActionOutcomeStatus { blocked, succeeded, failed, rolled_back, rollback_failed, outcome_unknown };

struct ActionOutcome {
    std::string audit_id;
    std::string plan_id;
    std::string plan_digest;
    ActionOutcomeStatus status{ActionOutcomeStatus::blocked};
    std::uint64_t occurred_at_ms{};
    std::string authorized_by{"none"};
    std::vector<std::string> effects;
    std::string error_code;
};

struct ActionPreparation {
    ActionPlan plan;
    ActionSimulation simulation;
    ActionPolicyDecision policy;
    bool confirmation_required{true};
    bool ready_for_confirmation{false};
};

class SupervisedActionController {
public:
    using EventSink = std::function<void(const CanonicalEvent&)>;

    SupervisedActionController(ActionPort* action_port, ActionPolicy& policy, EventSink event_sink)
        : action_port_(action_port), policy_(policy), event_sink_(std::move(event_sink)) {
        descriptor_.capability_id = "actuation.supervised";
        descriptor_.implementation_id = "local.supervised_actions";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "actuator";
        descriptor_.provides.push_back({"prepare.action", "urn:eu-digital:action-simulation:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
        descriptor_.permissions = {"action.explicit_confirmation"};
    }

    const CapabilityDescriptor& descriptor() const { return descriptor_; }
    const std::vector<ActionOutcome>& history() const { return history_; }

    ActionPreparation prepare(const ActionPlan& plan, std::uint64_t now_ms) {
        if (!plan.valid()) throw std::invalid_argument("invalid ActionPlan");
        ActionPreparation preparation;
        preparation.plan = plan;
        preparation.confirmation_required = true;
        if (action_port_ == nullptr) {
            preparation.policy = {false, "capability_absent", "no action provider is available"};
            prepared_[plan.plan_id] = {plan, {}, preparation.policy, std::nullopt, std::nullopt};
            return preparation;
        }

        try {
            preparation.simulation = action_port_->simulate(plan);
        } catch (const std::exception& error) {
            preparation.policy = {false, "simulation_failed", error.what()};
            prepared_[plan.plan_id] = {plan, {}, preparation.policy, std::nullopt, std::nullopt};
            return preparation;
        }
        if (!preparation.simulation.valid_for(plan)) {
            preparation.policy = {false, "invalid_simulation", "simulation does not match the plan"};
            prepared_[plan.plan_id] = {plan, preparation.simulation, preparation.policy, std::nullopt, std::nullopt};
            return preparation;
        }
        preparation.policy = policy_.evaluate(plan, preparation.simulation);
        preparation.ready_for_confirmation = preparation.policy.allowed;
        prepared_[plan.plan_id] = {
            plan, preparation.simulation, preparation.policy, std::nullopt, std::nullopt};
        if (!preparation.ready_for_confirmation) {
            record_blocked(plan, now_ms, preparation.policy.reason.empty()
                ? "policy_denied" : preparation.policy.reason);
        }
        return preparation;
    }

    bool authorize(const std::string& plan_id, const ActionAuthorization& authorization,
                   std::uint64_t now_ms) {
        auto found = prepared_.find(plan_id);
        if (found == prepared_.end()) return false;
        auto& state = found->second;
        if (!state.policy.allowed || !authorization.valid_at(now_ms) ||
            authorization.plan_id != state.plan.plan_id ||
            authorization.plan_digest != state.plan.plan_digest) {
            record_blocked(state.plan, now_ms, "invalid_authorization");
            return false;
        }
        state.authorization = authorization;
        emit("action.authorized", "{\"schema_version\":\"1.0\",\"plan_id\":\"" +
            json_escape(plan_id) + "\",\"authorization_id\":\"" +
            json_escape(authorization.authorization_id) + "\"}", now_ms);
        return true;
    }

    ActionOutcome execute(const std::string& plan_id, std::uint64_t now_ms) {
        auto found = prepared_.find(plan_id);
        if (found == prepared_.end()) return blocked_unknown(plan_id, now_ms, "unknown_plan");
        auto& state = found->second;
        if (!state.authorization) return record_blocked(state.plan, now_ms, "confirmation_required");
        const std::string authorized_by = state.authorization->authorized_by;
        const ActionAuthorization authorization = *state.authorization;
        state.authorization.reset();
        if (action_port_ == nullptr) return record_blocked(state.plan, now_ms, "capability_absent");

        ActionOutcome outcome;
        outcome.audit_id = next_audit_id();
        outcome.plan_id = state.plan.plan_id;
        outcome.plan_digest = state.plan.plan_digest;
        outcome.occurred_at_ms = now_ms;
        outcome.authorized_by = authorized_by;
        try {
            const auto result = action_port_->execute(state.plan);
            outcome.effects = result.effects;
            if (result.succeeded) {
                outcome.status = ActionOutcomeStatus::succeeded;
                state.last_outcome = outcome;
            } else {
                outcome.status = ActionOutcomeStatus::failed;
                outcome.error_code = result.error_code.empty() ? "execution_failed" : result.error_code;
            }
        } catch (const std::exception& error) {
            outcome.status = ActionOutcomeStatus::failed;
            outcome.error_code = error.what();
        }
        history_.push_back(outcome);
        emit_audit(outcome);
        return outcome;
    }

    ActionOutcome rollback(const std::string& plan_id, std::uint64_t now_ms) {
        auto found = prepared_.find(plan_id);
        if (found == prepared_.end()) return blocked_unknown(plan_id, now_ms, "unknown_plan");
        auto& state = found->second;
        if (!state.last_outcome || state.last_outcome->status != ActionOutcomeStatus::succeeded) {
            return record_blocked(state.plan, now_ms, "no_successful_action_to_rollback");
        }
        ActionOutcome outcome = *state.last_outcome;
        outcome.audit_id = next_audit_id();
        outcome.occurred_at_ms = now_ms;
        if (!state.simulation.reversible) {
            outcome.status = ActionOutcomeStatus::rollback_failed;
            outcome.error_code = "action_not_reversible";
        } else {
            try {
                if (action_port_ != nullptr && action_port_->rollback(state.plan)) {
                    outcome.status = ActionOutcomeStatus::rolled_back;
                    outcome.error_code.clear();
                } else {
                    outcome.status = ActionOutcomeStatus::rollback_failed;
                    outcome.error_code = "rollback_unavailable";
                }
            } catch (const std::exception& error) {
                outcome.status = ActionOutcomeStatus::rollback_failed;
                outcome.error_code = error.what();
            }
        }
        state.last_outcome = outcome;
        history_.push_back(outcome);
        emit_audit(outcome);
        return outcome;
    }

    ActionOutcome expire(const std::string& plan_id, std::uint64_t now_ms) {
        auto found = prepared_.find(plan_id);
        if (found == prepared_.end()) return blocked_unknown(plan_id, now_ms, "unknown_plan");
        auto& state = found->second;
        if (state.last_outcome) {
            return record_blocked(state.plan, now_ms, "action_already_has_outcome");
        }
        ActionOutcome outcome;
        outcome.audit_id = next_audit_id();
        outcome.plan_id = state.plan.plan_id;
        outcome.plan_digest = state.plan.plan_digest;
        outcome.status = ActionOutcomeStatus::outcome_unknown;
        outcome.occurred_at_ms = now_ms;
        outcome.error_code = "authorization_expired_or_system_crashed";
        state.last_outcome = outcome;
        history_.push_back(outcome);
        emit_audit(outcome);
        return outcome;
    }

private:
    struct PreparedState {
        ActionPlan plan;
        ActionSimulation simulation;
        ActionPolicyDecision policy;
        std::optional<ActionAuthorization> authorization;
        std::optional<ActionOutcome> last_outcome;
    };

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    static const char* status_name(ActionOutcomeStatus status) {
        switch (status) {
        case ActionOutcomeStatus::blocked: return "blocked";
        case ActionOutcomeStatus::succeeded: return "succeeded";
        case ActionOutcomeStatus::failed: return "failed";
        case ActionOutcomeStatus::rolled_back: return "rolled_back";
        case ActionOutcomeStatus::rollback_failed: return "rollback_failed";
        case ActionOutcomeStatus::outcome_unknown: return "outcome_unknown";
        }
        return "failed";
    }

    std::string next_audit_id() { return "action-audit-" + std::to_string(next_audit_id_++); }

    ActionOutcome record_blocked(const ActionPlan& plan, std::uint64_t now_ms, const std::string& reason) {
        ActionOutcome outcome;
        outcome.audit_id = next_audit_id();
        outcome.plan_id = plan.plan_id;
        outcome.plan_digest = plan.plan_digest;
        outcome.status = ActionOutcomeStatus::blocked;
        outcome.occurred_at_ms = now_ms;
        outcome.error_code = reason;
        history_.push_back(outcome);
        emit_audit(outcome);
        return outcome;
    }

    ActionOutcome blocked_unknown(const std::string& plan_id, std::uint64_t now_ms,
                                  const std::string& reason) {
        ActionPlan plan;
        plan.plan_id = plan_id;
        plan.plan_digest = "unknown";
        return record_blocked(plan, now_ms, reason);
    }

    void emit_audit(const ActionOutcome& outcome) {
        std::ostringstream payload;
        payload << "{\"schema_version\":\"1.0\",\"audit_id\":\"" << json_escape(outcome.audit_id)
                << "\",\"plan_id\":\"" << json_escape(outcome.plan_id)
                << "\",\"plan_digest\":\"" << json_escape(outcome.plan_digest)
                << "\",\"status\":\"" << status_name(outcome.status)
                << "\",\"occurred_at_ms\":" << outcome.occurred_at_ms
                << ",\"authorized_by\":\"" << json_escape(outcome.authorized_by) << "\",\"effects\":[";
        for (std::size_t index = 0; index < outcome.effects.size(); ++index) {
            if (index != 0) payload << ',';
            payload << "\"" << json_escape(outcome.effects[index]) << "\"";
        }
        payload << "]";
        if (!outcome.error_code.empty()) {
            payload << ",\"error_code\":\"" << json_escape(outcome.error_code) << "\"";
        }
        payload << "}";
        emit("action.audit", payload.str(), outcome.occurred_at_ms);
        emit("action_outcome", payload.str(), outcome.occurred_at_ms);
    }

    void emit(const std::string& event_type, const std::string& payload, std::uint64_t timestamp_ms) {
        if (!event_sink_) return;
        CanonicalEvent event;
        event.event_id = "action-controller-" + std::to_string(next_event_id_++);
        event.source = "supervised_action_controller";
        event.event_type = event_type;
        event.payload = payload;
        event.monotonic_ns = timestamp_ms * 1'000'000;
        event_sink_(event);
    }

    ActionPort* action_port_{};
    ActionPolicy& policy_;
    EventSink event_sink_;
    CapabilityDescriptor descriptor_;
    std::map<std::string, PreparedState> prepared_;
    std::vector<ActionOutcome> history_;
    std::uint64_t next_audit_id_{1};
    std::uint64_t next_event_id_{1};
};

class SupervisedActionPlugin final : public CapabilityPlugin {
public:
    SupervisedActionPlugin(ActionPort* action_port, ActionPolicy& policy,
                           SupervisedActionController::EventSink event_sink)
        : controller_(action_port, policy, std::move(event_sink)) {}

    const CapabilityDescriptor& descriptor() const override { return controller_.descriptor(); }
    void validate_manifest() override {
        if (!descriptor().valid()) throw CapabilityLifecycleError("invalid supervised action descriptor");
    }
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

    ActionPreparation prepare(const ActionPlan& plan, std::uint64_t now_ms) {
        return controller_.prepare(plan, now_ms);
    }
    bool authorize(const std::string& plan_id, const ActionAuthorization& authorization,
                   std::uint64_t now_ms) {
        return controller_.authorize(plan_id, authorization, now_ms);
    }
    ActionOutcome execute(const std::string& plan_id, std::uint64_t now_ms) {
        return controller_.execute(plan_id, now_ms);
    }
    ActionOutcome rollback(const std::string& plan_id, std::uint64_t now_ms) {
        return controller_.rollback(plan_id, now_ms);
    }
    ActionOutcome expire(const std::string& plan_id, std::uint64_t now_ms) {
        return controller_.expire(plan_id, now_ms);
    }
    const std::vector<ActionOutcome>& history() const { return controller_.history(); }

private:
    SupervisedActionController controller_;
};

}  // namespace eu_digital
