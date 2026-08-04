#pragma once

#include "core/ports/icognitive_decision_port.hpp"
#include "core/suggestion_orchestrator.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class CognitiveDecisionAdapter final : public ICognitiveDecisionPort {
public:
    explicit CognitiveDecisionAdapter(
        std::shared_ptr<SuggestionOrchestrator> orchestrator)
        : orchestrator_(std::move(orchestrator)) {
        if (!orchestrator_) {
            throw std::invalid_argument("orchestrator cannot be null");
        }
    }

    CognitiveDecision decide(
        const CanonicalEvent&, const CognitiveCycleContext&) override {
        throw std::invalid_argument(
            "legacy decision inputs lack suggestion evidence fields");
    }

    CognitiveDecision decide_evidence(
        const contracts::DecisionRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid decision request");
        }
        std::lock_guard lock(mutex_);

        SuggestionEvidence evidence;
        evidence.hypothesis_id = request.hypothesis_id;
        evidence.confidence = request.confidence;
        evidence.information_gain = request.information_gain;
        evidence.evidence_ids = request.evidence_ids;
        evidence.reason = request.reason;
        append_reference(evidence.evidence_ids, request.event_id);
        if (request.workspace_snapshot_id) {
            append_reference(evidence.evidence_ids, *request.workspace_snapshot_id);
        }

        const auto decision =
            orchestrator_->evaluate(evidence, request.occurred_at);
        if (decision.suppressed) {
            return CognitiveDecision::ok("silence", decision.reason, "");
        }
        if (request.event_type == "user_explicit_question") {
            return CognitiveDecision::ok(
                "requested_response", decision.reason, decision.decision_id);
        }
        if (request.event_type == "system_observation") {
            return CognitiveDecision::ok(
                "activity_detected", decision.reason, decision.decision_id);
        }
        return CognitiveDecision::ok(
            "proactive_suggestion", decision.reason, decision.decision_id);
    }

private:
    static void append_reference(
        std::vector<std::string>& references, const std::string& reference) {
        if (std::find(references.begin(), references.end(), reference) ==
            references.end()) {
            references.push_back(reference);
        }
    }

    std::shared_ptr<SuggestionOrchestrator> orchestrator_;
    std::mutex mutex_;
};

}  // namespace eu_digital
