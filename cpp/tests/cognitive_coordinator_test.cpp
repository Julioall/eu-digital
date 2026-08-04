#include "core/cognitive_coordinator.hpp"
#include "core/adapters/episode_segmenter_adapter.hpp"
#include "core/adapters/world_model_adapter.hpp"
#include "core/ports/icognitive_decision_port.hpp"
#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/ihypothesis_formation_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/iobservation_feature_port.hpp"
#include "core/ports/ipattern_learning_port.hpp"
#include "core/ports/iprediction_port.hpp"
#include "core/ports/isalience_assessment_port.hpp"
#include "core/ports/iself_model_query_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace eu_digital;

namespace {

struct Trace {
    void add(std::string value) {
        std::lock_guard lock(mutex);
        calls.push_back(std::move(value));
    }
    std::mutex mutex;
    std::vector<std::string> calls;
};

contracts::CognitiveCycleInputV1 input(std::string event_id = "event-1") {
    contracts::CognitiveCycleInputV1 value;
    value.correlation_id = "correlation-" + event_id;
    value.event_id = std::move(event_id);
    value.source = "system.activity";
    value.event_type = "system_observation";
    value.session_id = "session-1";
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.epoch_seconds = 1.0;
    value.modality = "system_activity";
    value.application = "editor";
    value.content = {{"process", "editor"}};
    return value;
}

contracts::EpisodeData episode_data() {
    contracts::EpisodeData value;
    value.episode_id = "episode-1";
    value.session_id = "session-1";
    value.start_at = "2026-08-04T12:00:00Z";
    value.end_at = "2026-08-04T12:00:00Z";
    value.event_ids = {"event-1"};
    value.applications = {"editor"};
    value.modalities = {"system_activity"};
    value.boundary_reasons = {"episode_start"};
    value.created_by = "fixture";
    return value;
}

class EpisodeFixture final : public IEpisodeBoundaryPort {
public:
    explicit EpisodeFixture(Trace& trace) : trace_(trace) {}
    EpisodeUpdate evaluate(const CanonicalEvent&) override { return {}; }
    contracts::PortResult<contracts::EpisodeSegmentationResponseV1>
    segment_observation(const contracts::EpisodeObservationRequest&,
                        const contracts::PortInvocationContextV1&) override {
        trace_.add("episode");
        contracts::EpisodeSegmentationResponseV1 value;
        value.update = {"episode-1", true, "active"};
        value.materialized_episode = episode_data();
        value.start_epoch = 1.0;
        value.end_epoch = 1.0;
        return contracts::PortResult<contracts::EpisodeSegmentationResponseV1>::ok(
            std::move(value));
    }
private:
    Trace& trace_;
};

class MemoryFixture final : public IMemoryWritePort, public IMemoryRetrievalPort {
public:
    explicit MemoryFixture(Trace& trace) : trace_(trace) {}
    MemoryWriteResult store_event(const CanonicalEvent&) override { return {}; }
    MemoryWriteResult store_episode(const contracts::EpisodeWriteRequest&) override {
        return MemoryWriteResult::ok("episode-1");
    }
    RetrievedMemorySet retrieve(const std::string&, int) override { return {}; }
    contracts::MemoryRetrievalResponse retrieve_memory(
        const contracts::MemoryRetrievalRequest&) override { return {}; }
    contracts::PortResult<MemoryWriteResult> store_episode_context(
        const contracts::EpisodeWriteRequest& request,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("memory_write");
        assert(request.episode.episode_id == "episode-1");
        return contracts::PortResult<MemoryWriteResult>::ok(
            MemoryWriteResult::ok("episode-1"));
    }
    contracts::PortResult<contracts::MemoryRetrievalResponse>
    retrieve_memory_context(const contracts::MemoryRetrievalRequest&,
                            const contracts::PortInvocationContextV1&) override {
        trace_.add("memory_retrieval");
        contracts::MemoryRetrievalResponse response;
        contracts::MemoryRetrievalItem item;
        item.memory_id = "memory-1";
        item.session_id = "session-1";
        item.event_ids = {"event-1"};
        response.items.push_back(std::move(item));
        return contracts::PortResult<contracts::MemoryRetrievalResponse>::ok(
            std::move(response));
    }
private:
    Trace& trace_;
};

class FeaturesFixture final : public IObservationFeaturePort {
public:
    explicit FeaturesFixture(Trace& trace) : trace_(trace) {}
    contracts::PortResult<contracts::ObservationFeaturesV1> extract_features(
        const contracts::CognitiveCycleInputV1& request,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("features");
        contracts::ObservationFeaturesV1 result;
        result.features = {{"cpu", 0.25}};
        result.evidence_refs = {request.event_id};
        return contracts::PortResult<contracts::ObservationFeaturesV1>::ok(
            std::move(result));
    }
private:
    Trace& trace_;
};

class PatternFixture final : public IPatternLearningPort {
public:
    explicit PatternFixture(Trace& trace) : trace_(trace) {}
    contracts::PatternLearningResult observe(
        const contracts::PatternLearningObservation&) override {
        return contracts::PatternLearningResult::failed("unused", "unused", "unused");
    }
    contracts::PatternLearningResult feedback(
        const contracts::PatternLearningFeedback&) override {
        return contracts::PatternLearningResult::failed("unused", "unused", "unused");
    }
    std::vector<contracts::ObservedPattern> snapshot() const override { return {}; }
    contracts::PatternLearningResult observe_context(
        const contracts::PatternLearningObservation& observation,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("pattern");
        assert(observation.features.at("cpu") == 0.25);
        contracts::ObservedPattern pattern;
        pattern.pattern_id = "pattern-1";
        pattern.created_by = "fixture";
        return contracts::PatternLearningResult::ok(std::move(pattern));
    }
private:
    Trace& trace_;
};

class PredictionFixture final : public IPredictionPort {
public:
    enum class Mode { success, failure, invalid_contract, cooperative_timeout };
    PredictionFixture(Trace& trace, Mode mode = Mode::success)
        : trace_(trace), mode_(mode) {}
    PredictionAssessment predict(const std::vector<std::string>&,
                                 const std::string&,
                                 const std::vector<std::string>&) override { return {}; }
    PredictionAssessment score(const PredictionAssessment&, const std::string&,
                               const std::string&) override { return {}; }
    contracts::PortResult<PredictionAssessment> predict_context(
        const std::vector<std::string>&, const std::string& predicted_at,
        const std::vector<std::string>&,
        const contracts::PortInvocationContextV1& context) override {
        trace_.add("prediction");
        if (mode_ == Mode::failure) {
            return contracts::PortResult<PredictionAssessment>::failed(
                "prediction.predict", "fixture_failure", "prediction failed");
        }
        if (mode_ == Mode::invalid_contract) {
            return contracts::PortResult<PredictionAssessment>::ok(
                PredictionAssessment{});
        }
        if (mode_ == Mode::cooperative_timeout) {
            while (!context.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return contracts::PortResult<PredictionAssessment>::failed(
                "prediction.predict", "cancelled", "deadline reached");
        }
        PredictionAssessment result;
        result.prediction_id = "prediction-1";
        result.stream_id = "session-1";
        result.model_id = "fixture";
        result.context = {"system_observation"};
        result.predicted_distribution = {{"next", 1.0}};
        result.predicted_at = predicted_at;
        result.top_k = 1;
        result.confidence = 0.7;
        return contracts::PortResult<PredictionAssessment>::ok(std::move(result));
    }
private:
    Trace& trace_;
    Mode mode_;
};

class SalienceFixture final : public ISalienceAssessmentPort {
public:
    explicit SalienceFixture(Trace& trace) : trace_(trace) {}
    contracts::PortResult<contracts::SalienceAssessmentV1> assess_salience(
        const contracts::SalienceAssessmentRequestV1& request,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("salience");
        contracts::SalienceAssessmentV1 result;
        result.signals = {{"novelty", 0.5}};
        result.evidence_refs = {request.input.event_id};
        return contracts::PortResult<contracts::SalienceAssessmentV1>::ok(
            std::move(result));
    }
private:
    Trace& trace_;
};

class WorkspaceFixture final : public IWorkspaceSelectionPort {
public:
    explicit WorkspaceFixture(Trace& trace) : trace_(trace) {}
    contracts::WorkspaceSnapshot select(const CanonicalEvent&) override { return {}; }
    contracts::WorkspaceAssessment select_candidate(
        const contracts::WorkspaceSelectionRequest&) override { return {}; }
    contracts::PortResult<contracts::WorkspaceAssessment> select_candidate_context(
        const contracts::WorkspaceSelectionRequest& request,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("workspace");
        contracts::WorkspaceAssessment result;
        result.snapshot_id = "snapshot-1";
        result.workspace_id = "workspace-1";
        result.session_id = request.session_id;
        result.created_at = request.observed_at;
        result.capacity = 4;
        result.policy_id = "fixture";
        result.config_fingerprint = "fixture";
        result.active_candidate_ids = {request.candidate_id};
        return contracts::PortResult<contracts::WorkspaceAssessment>::ok(
            std::move(result));
    }
private:
    Trace& trace_;
};

class HypothesisFixture final : public IHypothesisFormationPort {
public:
    explicit HypothesisFixture(Trace& trace) : trace_(trace) {}
    contracts::PortResult<contracts::HypothesisFormationResultV1> form_hypothesis(
        const contracts::HypothesisFormationRequestV1& request,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("hypothesis");
        contracts::HypothesisData hypothesis;
        hypothesis.hypothesis_id = "hypothesis-1";
        hypothesis.kind = "predictive";
        hypothesis.statement = "A next state may follow";
        hypothesis.confidence = request.prediction ? request.prediction->confidence : 0.5;
        hypothesis.supporting_refs = {request.input.event_id};
        hypothesis.alternatives = {"no_transition"};
        hypothesis.created_at = request.input.occurred_at;
        hypothesis.updated_at = request.input.occurred_at;
        hypothesis.expected_information_gain = 0.4;
        hypothesis.provenance_module = "fixture";
        contracts::HypothesisFormationResultV1 result;
        result.outcome = "formed";
        result.hypothesis = std::move(hypothesis);
        result.evidence_refs = {request.input.event_id};
        return contracts::PortResult<contracts::HypothesisFormationResultV1>::ok(
            std::move(result));
    }
private:
    Trace& trace_;
};

class MetacognitionFixture final : public IMetacognitionPort {
public:
    explicit MetacognitionFixture(Trace& trace) : trace_(trace) {}
    contracts::MetacognitiveAssessment evaluate(
        const contracts::WorkspaceSnapshot&) override { return {}; }
    contracts::MetacognitivePortAssessment evaluate_hypothesis(
        const contracts::MetacognitionRequest&) override { return {}; }
    contracts::PortResult<contracts::MetacognitivePortAssessment>
    evaluate_hypothesis_context(const contracts::MetacognitionRequest& request,
                                const contracts::PortInvocationContextV1&) override {
        trace_.add("metacognition");
        contracts::MetacognitivePortAssessment result;
        result.assessment_id = "assessment-1";
        result.hypothesis_id = request.hypothesis.hypothesis_id;
        result.evaluated_at = request.evaluated_at;
        result.curiosity_score = 0.5;
        result.requires_exploration = true;
        result.focus_area = request.hypothesis.hypothesis_id;
        return contracts::PortResult<contracts::MetacognitivePortAssessment>::ok(
            std::move(result));
    }
private:
    Trace& trace_;
};

class SelfModelFixture final : public ISelfModelQueryPort {
public:
    explicit SelfModelFixture(Trace& trace) : trace_(trace) {}
    SelfConstraintSnapshot query_constraints() override { return {}; }
    contracts::PortResult<SelfConstraintSnapshot> query_constraints_context(
        const contracts::PortInvocationContextV1&) override {
        trace_.add("self_model");
        SelfConstraintSnapshot result;
        result.model_id = "self-1";
        result.alignment_score = 1.0;
        return contracts::PortResult<SelfConstraintSnapshot>::ok(std::move(result));
    }
private:
    Trace& trace_;
};

class DecisionFixture final : public ICognitiveDecisionPort {
public:
    explicit DecisionFixture(Trace& trace) : trace_(trace) {}
    CognitiveDecision decide(const CanonicalEvent&,
                             const CognitiveCycleContext&) override { return {}; }
    CognitiveDecision decide_evidence(const contracts::DecisionRequest&) override {
        return {};
    }
    contracts::PortResult<CognitiveDecision> decide_evidence_context(
        const contracts::DecisionRequest&,
        const contracts::PortInvocationContextV1&) override {
        trace_.add("decision");
        return contracts::PortResult<CognitiveDecision>::ok(
            CognitiveDecision::ok("proactive_suggestion", "fixture", "decision-1"));
    }
private:
    Trace& trace_;
};

struct Fixtures {
    Trace trace;
    std::shared_ptr<EpisodeFixture> episode{std::make_shared<EpisodeFixture>(trace)};
    std::shared_ptr<MemoryFixture> memory{std::make_shared<MemoryFixture>(trace)};
    std::shared_ptr<FeaturesFixture> features{std::make_shared<FeaturesFixture>(trace)};
    std::shared_ptr<PatternFixture> pattern{std::make_shared<PatternFixture>(trace)};
    std::shared_ptr<PredictionFixture> prediction{
        std::make_shared<PredictionFixture>(trace)};
    std::shared_ptr<SalienceFixture> salience{std::make_shared<SalienceFixture>(trace)};
    std::shared_ptr<WorkspaceFixture> workspace{std::make_shared<WorkspaceFixture>(trace)};
    std::shared_ptr<HypothesisFixture> hypothesis{
        std::make_shared<HypothesisFixture>(trace)};
    std::shared_ptr<MetacognitionFixture> metacognition{
        std::make_shared<MetacognitionFixture>(trace)};
    std::shared_ptr<SelfModelFixture> self_model{
        std::make_shared<SelfModelFixture>(trace)};
    std::shared_ptr<DecisionFixture> decision{std::make_shared<DecisionFixture>(trace)};

    void register_all(CapabilityRegistry& registry) {
        registry.register_instance<IEpisodeBoundaryPort>("episode_boundary", episode);
        registry.register_instance<IMemoryWritePort>("memory_write", memory);
        registry.register_instance<IMemoryRetrievalPort>("memory_retrieval", memory);
        registry.register_instance<IObservationFeaturePort>("observation_features", features);
        registry.register_instance<IPatternLearningPort>("pattern_learning", pattern);
        registry.register_instance<IPredictionPort>("prediction", prediction);
        registry.register_instance<ISalienceAssessmentPort>("salience", salience);
        registry.register_instance<IWorkspaceSelectionPort>("workspace", workspace);
        registry.register_instance<IHypothesisFormationPort>("hypothesis_formation", hypothesis);
        registry.register_instance<IMetacognitionPort>("metacognition", metacognition);
        registry.register_instance<ISelfModelQueryPort>("self_model", self_model);
        registry.register_instance<ICognitiveDecisionPort>("decision", decision);
    }
};

void full_sequence_is_exactly_once() {
    CapabilityRegistry registry;
    Fixtures fixtures;
    fixtures.register_all(registry);
    CognitiveCoordinator coordinator(registry);
    std::vector<contracts::CognitiveOutputRequestV1> output_requests;
    coordinator.set_cognitive_output_handler(
        [&](const auto& request) { output_requests.push_back(request); });
    assert(coordinator.enqueue_input(input()).status == EnqueueStatusV1::accepted);
    coordinator.wait_idle();
    coordinator.stop();
    const std::vector<std::string> expected{
        "episode", "memory_write", "memory_retrieval", "features", "pattern",
        "prediction", "salience", "workspace", "hypothesis", "metacognition",
        "self_model", "decision"};
    assert(fixtures.trace.calls == expected);
    const auto results = coordinator.results();
    assert(results.size() == 1);
    assert(results.front().state == contracts::CycleStateV1::completed);
    assert(results.front().decision.has_value());
    assert(results.front().valid());
    assert(output_requests.size() == 1);
    assert(output_requests.front().valid());
    assert(output_requests.front().intent ==
           contracts::CognitiveOutputIntentV1::proactive_suggestion);
    assert(std::find(output_requests.front().evidence_refs.begin(),
                     output_requests.front().evidence_refs.end(),
                     "memory-1") != output_requests.front().evidence_refs.end());
    assert(output_requests.front().self_model_id == "self-1");
}

void queue_duplicate_and_reentry_are_bounded() {
    CapabilityRegistry registry;
    CognitiveCoordinatorConfig config;
    config.max_queue_size = 2;
    config.auto_start = false;
    CognitiveCoordinator coordinator(registry, config);
    assert(coordinator.enqueue_input(input("one")).status == EnqueueStatusV1::accepted);
    assert(coordinator.enqueue_input(input("one")).status ==
           EnqueueStatusV1::discarded_duplicate);
    assert(coordinator.enqueue_input(input("two")).status == EnqueueStatusV1::accepted);
    assert(coordinator.enqueue_input(input("three")).status ==
           EnqueueStatusV1::discarded_backpressure);
    auto internal = input("internal");
    internal.event_type = "cognitive.cycle.result";
    assert(coordinator.enqueue_input(internal).status == EnqueueStatusV1::discarded_reentry);
    coordinator.start();
    coordinator.wait_idle();
    coordinator.stop();
    const auto results = coordinator.results();
    assert(results.size() == 4);
}

void prediction_failure_and_timeout_continue_to_decision() {
    for (const auto mode : {PredictionFixture::Mode::failure,
                            PredictionFixture::Mode::invalid_contract,
                            PredictionFixture::Mode::cooperative_timeout}) {
        CapabilityRegistry registry;
        Fixtures fixtures;
        fixtures.prediction = std::make_shared<PredictionFixture>(fixtures.trace, mode);
        fixtures.register_all(registry);
        CognitiveCoordinatorConfig config;
        config.stage_timeout = std::chrono::milliseconds(10);
        CognitiveCoordinator coordinator(registry, config);
        const auto event_id = mode == PredictionFixture::Mode::failure
            ? "failure"
            : (mode == PredictionFixture::Mode::invalid_contract
                   ? "invalid-contract"
                   : "timeout");
        coordinator.enqueue_input(input(event_id));
        coordinator.wait_idle();
        coordinator.stop();
        const auto result = coordinator.results().back();
        assert(result.state == contracts::CycleStateV1::degraded);
        assert(result.decision.has_value());
        assert(std::find(fixtures.trace.calls.begin(), fixtures.trace.calls.end(),
                         "decision") != fixtures.trace.calls.end());
        const auto prediction_stage = std::find_if(
            result.stages.begin(), result.stages.end(), [](const auto& stage) {
                return stage.stage_id == "prediction";
            });
        assert(prediction_stage != result.stages.end());
        if (mode == PredictionFixture::Mode::cooperative_timeout) {
            assert(prediction_stage->status == contracts::CycleStageStatusV1::timed_out);
        } else if (mode == PredictionFixture::Mode::invalid_contract) {
            assert(prediction_stage->status == contracts::CycleStageStatusV1::failed);
            assert(prediction_stage->error);
            assert(prediction_stage->error->code == "contract_violation");
        }
    }
}

void ablation_and_replay_are_safe() {
    CapabilityRegistry registry;
    Fixtures fixtures;
    fixtures.register_all(registry);
    CognitiveCoordinator coordinator(registry);
    auto replay_input = input("replay");
    replay_input.replay_mode = true;
    int published = 0;
    int output_requests = 0;
    coordinator.set_publisher([&](const CanonicalEvent&) { ++published; });
    coordinator.set_cognitive_output_handler(
        [&](const auto&) { ++output_requests; });
    coordinator.enqueue_input(replay_input);
    coordinator.wait_idle();
    coordinator.stop();
    const auto result = coordinator.results().back();
    assert(result.state == contracts::CycleStateV1::completed);
    assert(!result.decision.has_value());
    assert(published == 0);
    assert(output_requests == 0);
    assert(result.valid());
}

void coordinator_checkpoint_restores_idempotence_and_commits_once() {
    CapabilityRegistry registry;
    CognitiveCoordinator first(registry);
    std::atomic<int> commits{0};
    first.set_cycle_commit_handler(
        [&](const auto& committed_input, const auto& committed_result) {
            assert(committed_input.event_id == committed_result.input_event_id);
            ++commits;
        });
    assert(first.enqueue_input(input("checkpoint-event")).status ==
           EnqueueStatusV1::accepted);
    first.wait_idle();
    assert(commits == 1);
    const auto checkpoint = first.capture_checkpoint();
    assert(checkpoint.valid());
    assert(checkpoint.seen_event_ids ==
           std::vector<std::string>{"checkpoint-event"});
    first.stop();

    CognitiveCoordinatorConfig config;
    config.auto_start = false;
    CognitiveCoordinator restored(registry, config);
    assert(restored.restore_checkpoint(checkpoint));
    const auto before_invalid = restored.capture_checkpoint();
    auto incompatible = checkpoint;
    incompatible.policy_id = "different-policy";
    assert(!restored.restore_checkpoint(incompatible));
    assert(restored.capture_checkpoint().to_json() == before_invalid.to_json());
    restored.start();
    assert(restored.enqueue_input(input("checkpoint-event")).status ==
           EnqueueStatusV1::discarded_duplicate);
    assert(restored.enqueue_input(input("new-event")).status ==
           EnqueueStatusV1::accepted);
    restored.wait_idle();
    const auto continued = restored.capture_checkpoint();
    assert((continued.seen_event_ids ==
            std::vector<std::string>{"checkpoint-event", "new-event"}));
    restored.stop();
}

CapabilityDescriptor real_descriptor(std::string operation,
                                     std::string implementation_id) {
    CapabilityDescriptor descriptor;
    descriptor.capability_id = operation;
    descriptor.implementation_id = std::move(implementation_id);
    descriptor.implementation_version = "1.0.0";
    descriptor.kind = "adapter";
    descriptor.provides.push_back({std::move(operation), "1.0"});
    return descriptor;
}

void two_real_adapters_integrate_with_mocked_ports() {
    CapabilityRegistry registry;
    Fixtures fixtures;
    fixtures.register_all(registry);

    auto episode = std::make_shared<EpisodeSegmenterAdapter>();
    registry.register_instance<IEpisodeBoundaryPort>(
        real_descriptor("episode_boundary", "episode_boundary_real"), episode, 200);
    auto world_model = std::make_shared<WorldModel>(
        WorldModelConfig{}, "coordinator-integration");
    auto prediction = std::make_shared<WorldModelAdapter>(world_model);
    registry.register_instance<IPredictionPort>(
        real_descriptor("prediction", "prediction_real"), prediction, 200);

    CognitiveCoordinator coordinator(registry);
    assert(coordinator.enqueue_input(input("real-adapters")).status ==
           EnqueueStatusV1::accepted);
    coordinator.wait_idle();
    coordinator.stop();

    const auto result = coordinator.results().back();
    assert(result.state == contracts::CycleStateV1::completed);
    const auto episode_stage = std::find_if(
        result.stages.begin(), result.stages.end(), [](const auto& stage) {
            return stage.stage_id == "episode";
        });
    const auto prediction_stage = std::find_if(
        result.stages.begin(), result.stages.end(), [](const auto& stage) {
            return stage.stage_id == "prediction";
        });
    assert(episode_stage != result.stages.end());
    assert(prediction_stage != result.stages.end());
    assert(episode_stage->status == contracts::CycleStageStatusV1::succeeded);
    assert(prediction_stage->status == contracts::CycleStageStatusV1::succeeded);
    assert(!prediction_stage->evidence_refs.empty());
    assert(prediction_stage->evidence_refs.front() != "prediction-1");
}

}  // namespace

int main() {
    full_sequence_is_exactly_once();
    queue_duplicate_and_reentry_are_bounded();
    prediction_failure_and_timeout_continue_to_decision();
    ablation_and_replay_are_safe();
    coordinator_checkpoint_restores_idempotence_and_commits_once();
    two_real_adapters_integrate_with_mocked_ports();
}
