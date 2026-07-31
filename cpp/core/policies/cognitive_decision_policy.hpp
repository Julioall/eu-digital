#pragma once

#include "core/contracts/cognitive_output.hpp"
#include "core/suggestion_orchestrator.hpp"
#include "core/event_bus.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace eu_digital {

class CognitiveDecisionPolicy {
public:
    explicit CognitiveDecisionPolicy(std::shared_ptr<SuggestionOrchestrator> orchestrator)
        : orchestrator_(std::move(orchestrator)) {
        if (!orchestrator_) {
            throw std::invalid_argument("orchestrator cannot be null");
        }
    }

    CognitiveOutputRequest decide(const CanonicalEvent& event, const std::string& timestamp) {
        std::lock_guard lock(mutex_);

        // For user explicit questions/requests, we bypass the proactive budget
        // and do not reset the cooldown.
        if (event.event_type == "user_explicit_question" || event.event_type == "requested_response") {
            return CognitiveOutputRequest{
                "requested_response",
                "{}", // Self constraint snapshot
                {},   // Context memories
                "Prompt for requested response"
            };
        }

        // For proactive suggestions, we evaluate through the orchestrator.
        SuggestionEvidence evidence;
        evidence.hypothesis_id = "hyp-" + event.event_id;
        evidence.confidence = 0.8; 
        evidence.information_gain = 0.5;
        evidence.evidence_ids = {event.event_id};
        evidence.reason = "proactive evaluation";

        auto decision = orchestrator_->evaluate(evidence, timestamp);

        if (decision.suppressed) {
            return CognitiveOutputRequest{
                "silence",
                "{}",
                {},
                "Suppressed: " + decision.reason
            };
        }

        return CognitiveOutputRequest{
            "proactive_suggestion",
            "{}", // Snapshot
            {event.event_id}, // Memories
            "Prompt for proactive suggestion"
        };
    }

private:
    std::shared_ptr<SuggestionOrchestrator> orchestrator_;
    std::mutex mutex_;
};

} // namespace eu_digital
