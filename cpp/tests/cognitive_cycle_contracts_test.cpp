#include "core/contracts/cognitive_cycle_v1.hpp"

#include <cassert>
#include <chrono>
#include <stop_token>

using namespace eu_digital;
using namespace eu_digital::contracts;

namespace {

CognitiveCycleInputV1 input() {
    CognitiveCycleInputV1 value;
    value.correlation_id = "correlation-1";
    value.event_id = "event-1";
    value.source = "system.activity";
    value.event_type = "system_observation";
    value.session_id = "session-1";
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.epoch_seconds = 1.0;
    value.modality = "system_activity";
    value.application = "editor";
    value.content = {{"process", "editor"}};
    value.numeric_features = {{"cpu", 0.25}};
    value.salience_signals = {{"novelty", 0.5}};
    value.time_basis = "source_occurred";
    return value;
}

}  // namespace

int main() {
    auto cycle_input = input();
    assert(cycle_input.valid());
    cycle_input.time_basis = "invented";
    assert(!cycle_input.valid());

    PortInvocationContextV1 invocation;
    invocation.correlation_id = "correlation-1";
    invocation.deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(100);
    assert(invocation.valid());
    std::stop_source stop_source;
    invocation.stop_token = stop_source.get_token();
    stop_source.request_stop();
    assert(invocation.stop_requested());

    CognitiveCycleStageV1 stage;
    stage.stage_id = "prediction";
    stage.operation = "prediction.predict";
    stage.status = CycleStageStatusV1::succeeded;
    stage.evidence_refs = {"prediction-1"};
    assert(stage.valid());
    stage.error = PortError{"1.0", "prediction.predict", "failure", "failed", false};
    assert(!stage.valid());
    stage.error.reset();

    EpisodeSegmentationResponseV1 segmentation;
    segmentation.update.episode_id = "episode-1";
    segmentation.update.current_state = "active";
    assert(segmentation.valid());

    ObservationFeaturesV1 features;
    features.features = {{"cpu", 0.25}};
    features.evidence_refs = {"event-1"};
    assert(features.valid());

    SalienceAssessmentV1 salience;
    salience.signals = {{"novelty", 0.5}};
    salience.evidence_refs = {"event-1"};
    assert(salience.valid());

    HypothesisFormationResultV1 formation;
    formation.outcome = "insufficient_evidence";
    assert(formation.valid());

    CognitiveCycleResultV1 result;
    result.cycle_id = "cycle-1";
    result.correlation_id = "correlation-1";
    result.input_event_id = "event-1";
    result.state = CycleStateV1::completed;
    result.policy_id = "bounded_ports_v1";
    result.stages.push_back(stage);
    assert(result.valid());
    const auto json = result.to_json();
    assert(json.find("\"schema_version\":\"1.0\"") != std::string::npos);
    assert(json.find("card_id") == std::string::npos);
    assert(json.find("payload_text") == std::string::npos);
}
