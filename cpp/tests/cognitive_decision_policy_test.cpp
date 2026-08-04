#include "core/policies/cognitive_decision_policy.hpp"

#include <cassert>

using namespace eu_digital;

namespace {

contracts::CognitiveCycleInputV1 input() {
    contracts::CognitiveCycleInputV1 value;
    value.correlation_id = "correlation-1";
    value.event_id = "event-1";
    value.source = "user";
    value.event_type = "user_explicit_question";
    value.session_id = "session-1";
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.epoch_seconds = 1.0;
    value.modality = "text";
    value.content = {{"text", "O que aprendeu?"}};
    return value;
}

}  // namespace

int main() {
    const auto cycle_input = input();
    const auto decision = CognitiveDecision::ok(
        "requested_response", "explicit_user_request", "decision-1");

    contracts::EpisodeSegmentationResponseV1 episode;
    episode.update.episode_id = "episode-1";
    episode.update.current_state = "active";
    contracts::MemoryRetrievalResponse memories;
    contracts::MemoryRetrievalItem memory;
    memory.memory_id = "memory-1";
    memory.session_id = "session-1";
    memory.event_ids = {"event-1"};
    memories.items.push_back(memory);
    contracts::WorkspaceAssessment workspace;
    workspace.snapshot_id = "workspace-snapshot-1";
    workspace.workspace_id = "workspace-1";
    workspace.session_id = "session-1";
    workspace.created_at = cycle_input.occurred_at;
    workspace.capacity = 1;
    workspace.policy_id = "policy-1";
    workspace.config_fingerprint = "fingerprint-1";
    contracts::MetacognitivePortAssessment metacognition;
    metacognition.assessment_id = "assessment-1";
    metacognition.hypothesis_id = "hypothesis-1";
    metacognition.evaluated_at = cycle_input.occurred_at;
    metacognition.focus_area = "question";
    SelfConstraintSnapshot self_model;
    self_model.model_id = "self-1";
    self_model.active_constraints = {"local_only"};
    contracts::HypothesisData hypothesis;
    hypothesis.hypothesis_id = "hypothesis-1";
    hypothesis.kind = "contextual";
    hypothesis.statement = "user requested an answer";
    hypothesis.supporting_refs = {"event-1"};
    hypothesis.created_at = cycle_input.occurred_at;
    hypothesis.updated_at = cycle_input.occurred_at;
    hypothesis.provenance_module = "fixture";

    const auto request = CognitiveDecisionPolicy::create_request(
        cycle_input, decision, episode, memories, workspace, metacognition,
        self_model, hypothesis);
    assert(request);
    assert(request->valid());
    assert(request->critical);
    assert(request->input_content.at("text") == "O que aprendeu?");
    assert(request->self_constraints == std::vector<std::string>{"local_only"});
    assert(std::find(request->evidence_refs.begin(), request->evidence_refs.end(),
                     "memory-1") != request->evidence_refs.end());
    assert(std::find(request->evidence_refs.begin(), request->evidence_refs.end(),
                     "assessment-1") != request->evidence_refs.end());

    auto replay = cycle_input;
    replay.replay_mode = true;
    assert(!CognitiveDecisionPolicy::create_request(
        replay, decision, episode, memories, workspace, metacognition,
        self_model, hypothesis));
    assert(!CognitiveDecisionPolicy::create_request(
        cycle_input, CognitiveDecision::ok("activity_detected", "observed"),
        episode, memories, workspace, metacognition, self_model, hypothesis));
    assert(!CognitiveDecisionPolicy::create_request(
        cycle_input, CognitiveDecision::ok("invented", "invalid"), episode,
        memories, workspace, metacognition, self_model, hypothesis));
}
