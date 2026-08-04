#pragma once

#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/cognitive_output.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/self_constraint_snapshot.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <optional>
#include <string>

namespace eu_digital {

class CognitiveDecisionPolicy {
public:
    static std::optional<contracts::CognitiveOutputRequestV1> create_request(
        const contracts::CognitiveCycleInputV1& input,
        const CognitiveDecision& decision,
        const std::optional<contracts::EpisodeSegmentationResponseV1>& episode,
        const std::optional<contracts::MemoryRetrievalResponse>& memories,
        const std::optional<contracts::WorkspaceAssessment>& workspace,
        const std::optional<contracts::MetacognitivePortAssessment>& metacognition,
        const std::optional<SelfConstraintSnapshot>& self_model,
        const std::optional<contracts::HypothesisData>& hypothesis) {
        if (!input.valid() || input.replay_mode || !decision.success ||
            decision.reason.empty()) {
            return std::nullopt;
        }
        const auto intent = classify(decision.intent);
        if (!intent || *intent == contracts::CognitiveOutputIntentV1::silence) {
            return std::nullopt;
        }

        contracts::CognitiveOutputRequestV1 request;
        request.request_id = digest::uuid5(
            contracts::kCognitiveOutputNamespace,
            input.event_id + ":" + decision.intent + ":" +
                decision.target_action);
        request.correlation_id = input.correlation_id;
        request.input_event_id = input.event_id;
        request.intent = *intent;
        request.occurred_at = input.occurred_at;
        request.critical =
            *intent == contracts::CognitiveOutputIntentV1::question ||
            *intent == contracts::CognitiveOutputIntentV1::requested_response;
        request.reason = decision.reason;
        request.input_content = input.content;
        append(request.evidence_refs, input.event_id);
        append(request.evidence_refs, decision.target_action);

        if (episode) {
            append(request.evidence_refs, episode->update.episode_id);
            if (episode->materialized_episode) {
                append(request.evidence_refs,
                       episode->materialized_episode->episode_id);
            }
        }
        if (memories) {
            for (const auto& memory : memories->items) {
                append(request.evidence_refs, memory.memory_id);
            }
        }
        if (workspace) append(request.evidence_refs, workspace->snapshot_id);
        if (metacognition) {
            append(request.evidence_refs, metacognition->assessment_id);
        }
        if (hypothesis) {
            append(request.evidence_refs, hypothesis->hypothesis_id);
            for (const auto& reference : hypothesis->supporting_refs) {
                append(request.evidence_refs, reference);
            }
            for (const auto& reference : hypothesis->opposing_refs) {
                append(request.evidence_refs, reference);
            }
        }
        if (self_model) {
            request.self_model_id = self_model->model_id;
            request.self_constraints = self_model->active_constraints;
            append(request.evidence_refs, self_model->model_id);
        }

        if (!request.valid()) return std::nullopt;
        return request;
    }

    static std::optional<contracts::CognitiveOutputIntentV1> classify(
        const std::string& intent) {
        if (intent == "silence" || intent == "activity_detected") {
            return contracts::CognitiveOutputIntentV1::silence;
        }
        return contracts::cognitive_output_intent(intent);
    }

private:
    static void append(std::vector<std::string>& values,
                       const std::string& value) {
        contracts::cognitive_output_detail::append_unique(values, value);
    }
};

}  // namespace eu_digital
