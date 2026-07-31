#pragma once

#include "core/ports/icognitive_decision_port.hpp"
#include "core/suggestion_orchestrator.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class CognitiveDecisionAdapter final : public ICognitiveDecisionPort {
public:
    explicit CognitiveDecisionAdapter(std::shared_ptr<SuggestionOrchestrator> orchestrator)
        : orchestrator_(std::move(orchestrator)) {
        if (!orchestrator_) {
            throw std::invalid_argument("orchestrator cannot be null");
        }
    }

    CognitiveDecision decide(const CanonicalEvent& event) override {
        std::lock_guard lock(mutex_);
        
        SuggestionEvidence evidence;
        evidence.hypothesis_id = "hyp-from-event";
        evidence.confidence = 0.5;
        evidence.information_gain = 0.1;
        evidence.evidence_ids = {event.event_id};
        evidence.reason = "orchestrated from canonical event";
        
        // Pass a dummy timestamp since CanonicalEvent doesn't have occurred_at
        auto decision = orchestrator_->evaluate(evidence, "2026-07-31T12:00:00Z");
        
        if (decision.suppressed) {
            return CognitiveDecision::ok("silence", decision.reason, "");
        }
        return CognitiveDecision::ok("execute_action", decision.reason, decision.decision_id);
    }

private:
    std::shared_ptr<SuggestionOrchestrator> orchestrator_;
    std::mutex mutex_;
};

} // namespace eu_digital
