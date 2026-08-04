#include "core/contracts/cognitive_port_requests.hpp"

#include <cassert>

using namespace eu_digital::contracts;

namespace {

EpisodeData episode() {
    EpisodeData value;
    value.episode_id = "episode-1";
    value.session_id = "session-1";
    value.start_at = "2026-08-04T12:00:00Z";
    value.end_at = "2026-08-04T12:00:01Z";
    value.event_ids = {"event-1"};
    value.applications = {"editor"};
    value.modalities = {"system_activity"};
    value.boundary_reasons = {"episode_start"};
    value.created_by = "fixture";
    return value;
}

HypothesisData hypothesis() {
    HypothesisData value;
    value.hypothesis_id = "hypothesis-1";
    value.kind = "predictive";
    value.statement = "The editor remains active";
    value.status = "proposed";
    value.confidence = 0.7;
    value.supporting_refs = {"event-1"};
    value.created_at = "2026-08-04T12:00:00Z";
    value.updated_at = "2026-08-04T12:00:00Z";
    value.provenance_module = "fixture";
    return value;
}

}  // namespace

int main() {
    EpisodeObservationRequest observation;
    observation.event_id = "event-1";
    observation.session_id = "session-1";
    observation.occurred_at = "2026-08-04T12:00:00Z";
    observation.epoch_seconds = 1.0;
    observation.modality = "system_activity";
    assert(observation.valid());
    observation.schema_version = "2.0";
    assert(!observation.valid());

    EpisodeWriteRequest write;
    write.episode = episode();
    write.start_epoch = 1.0;
    write.end_epoch = 2.0;
    assert(write.valid());
    write.episode.event_ids.clear();
    assert(!write.valid());

    MemoryRetrievalRequest retrieval;
    retrieval.session_id = "session-1";
    retrieval.limit = 5;
    assert(retrieval.valid());
    retrieval.limit = 0;
    assert(!retrieval.valid());

    MemoryRetrievalResponse retrieval_response;
    assert(retrieval_response.valid());

    WorkspaceSelectionRequest workspace;
    workspace.candidate_id = "candidate-1";
    workspace.session_id = "session-1";
    workspace.source_kind = "canonical_event";
    workspace.source_refs = {"event-1"};
    workspace.observed_at = "2026-08-04T12:00:00Z";
    workspace.observed_epoch = 1.0;
    workspace.salience_signals = {{"novelty", 0.8}};
    assert(workspace.valid());

    MetacognitionRequest metacognition;
    metacognition.hypothesis = hypothesis();
    metacognition.evaluated_at = "2026-08-04T12:00:01Z";
    metacognition.workspace_snapshot_id = "snapshot-1";
    assert(metacognition.valid());

    MetacognitivePortAssessment assessment;
    assessment.assessment_id = "assessment-1";
    assessment.hypothesis_id = "hypothesis-1";
    assessment.evaluated_at = "2026-08-04T12:00:01Z";
    assessment.curiosity_score = 0.5;
    assessment.focus_area = "hypothesis-1";
    assert(assessment.valid());

    DecisionRequest decision;
    decision.event_id = "event-1";
    decision.event_type = "user_explicit_question";
    decision.occurred_at = "2026-08-04T12:00:02Z";
    decision.hypothesis_id = "hypothesis-1";
    decision.confidence = 0.7;
    decision.information_gain = 0.4;
    decision.evidence_ids = {"event-1"};
    decision.reason = "explicit user request";
    assert(decision.valid());
    decision.evidence_ids.clear();
    assert(!decision.valid());
}
