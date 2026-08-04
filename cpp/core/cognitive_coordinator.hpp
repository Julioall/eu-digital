#pragma once

#include "core/capability_runtime.hpp"
#include "core/contracts/cognitive_cycle_context.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/digest.hpp"
#include "core/event_bus.hpp"
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
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr char COGNITIVE_CYCLE_NAMESPACE[] =
    "78398845-173c-4c66-bf18-27f20dcab61e";
inline constexpr char COGNITIVE_COORDINATOR_SOURCE[] = "cognitive.coordinator";

// Legacy observable state retained for source compatibility.
enum class CycleState { queued, processing, degraded, completed, failed, discarded };

struct CycleStatusLog {
    std::string event_id;
    CycleState state;
    std::string reason;
};

enum class EnqueueStatusV1 {
    accepted,
    discarded_duplicate,
    discarded_backpressure,
    discarded_reentry,
    discarded_invalid,
    discarded_stopped
};

struct EnqueueReceiptV1 {
    EnqueueStatusV1 status{EnqueueStatusV1::discarded_invalid};
    std::string cycle_id;
};

struct CognitiveCoordinatorConfig {
    std::size_t max_queue_size{100};
    std::chrono::milliseconds stage_timeout{100};
    bool auto_start{true};

    void validate() const {
        if (max_queue_size == 0) {
            throw std::invalid_argument("max_queue_size must be positive");
        }
        if (stage_timeout <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument("stage_timeout must be positive");
        }
    }
};

class CognitiveCoordinator {
public:
    explicit CognitiveCoordinator(
        const CapabilityRegistry& registry,
        CognitiveCoordinatorConfig config = {})
        : registry_(registry), config_(config) {
        config_.validate();
        if (config_.auto_start) start();
    }

    // Compatibility constructor for the pre-1.0 coordinator API.
    CognitiveCoordinator(const CapabilityRegistry& registry, std::size_t max_queue_size)
        : CognitiveCoordinator(
              registry, CognitiveCoordinatorConfig{max_queue_size,
                                                    std::chrono::milliseconds(100),
                                                    true}) {}

    ~CognitiveCoordinator() { stop(); }

    CognitiveCoordinator(const CognitiveCoordinator&) = delete;
    CognitiveCoordinator& operator=(const CognitiveCoordinator&) = delete;

    void start() {
        std::lock_guard lock(queue_mutex_);
        if (started_) return;
        if (stopping_) throw std::logic_error("coordinator cannot restart after stop");
        started_ = true;
        worker_ = std::thread(&CognitiveCoordinator::process_loop, this);
    }

    void set_publisher(std::function<void(const CanonicalEvent&)> publisher) {
        std::lock_guard lock(publisher_mutex_);
        publisher_ = std::move(publisher);
    }

    EnqueueReceiptV1 enqueue_input(const contracts::CognitiveCycleInputV1& input) {
        const auto cycle_id = make_cycle_id(input.event_id);
        if (!input.valid()) {
            append_discarded(input, cycle_id, "invalid_cycle_input");
            return {EnqueueStatusV1::discarded_invalid, cycle_id};
        }
        if (input.event_type == "cognitive.cycle.result" ||
            std::find(input.causation_chain.begin(), input.causation_chain.end(),
                      COGNITIVE_COORDINATOR_SOURCE) != input.causation_chain.end()) {
            append_discarded(input, cycle_id, "internal_reentry");
            return {EnqueueStatusV1::discarded_reentry, cycle_id};
        }

        {
            std::lock_guard lock(queue_mutex_);
            if (stopping_) {
                append_discarded_locked(input, cycle_id, "coordinator_stopped");
                return {EnqueueStatusV1::discarded_stopped, cycle_id};
            }
            if (seen_event_ids_.contains(input.event_id)) {
                log_state(input.event_id, CycleState::discarded, "duplicate_event");
                return {EnqueueStatusV1::discarded_duplicate, cycle_id};
            }
            if (queue_.size() >= config_.max_queue_size) {
                seen_event_ids_.insert(input.event_id);
                append_discarded_locked(input, cycle_id, "queue_full_backpressure");
                return {EnqueueStatusV1::discarded_backpressure, cycle_id};
            }
            seen_event_ids_.insert(input.event_id);
            queue_.push_back(input);
            log_state(input.event_id, CycleState::queued, "");
        }
        queue_ready_.notify_one();
        return {EnqueueStatusV1::accepted, cycle_id};
    }

    // Legacy events do not carry the fields required by CognitiveCycleInputV1.
    // They are rejected without publishing a recursive result.
    void enqueue(const CanonicalEvent& event) {
        contracts::CognitiveCycleInputV1 input;
        input.correlation_id = event.event_id;
        input.event_id = event.event_id;
        input.source = event.source;
        input.event_type = event.event_type;
        enqueue_input(input);
    }

    void wait_idle() {
        std::unique_lock lock(queue_mutex_);
        idle_.wait(lock, [this] { return queue_.empty() && in_flight_ == 0; });
    }

    void stop() {
        {
            std::lock_guard lock(queue_mutex_);
            if (stopping_) {
                if (!worker_.joinable()) return;
            } else {
                stopping_ = true;
            }
        }
        queue_ready_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    std::vector<contracts::CognitiveCycleResultV1> results() const {
        std::lock_guard lock(result_mutex_);
        return results_;
    }

    std::vector<CycleStatusLog> get_logs() const {
        std::lock_guard lock(log_mutex_);
        return logs_;
    }

    CognitiveCycleContext last_cycle_context() const {
        std::lock_guard lock(context_mutex_);
        return last_context_;
    }

private:
    struct CycleValues {
        std::optional<contracts::EpisodeSegmentationResponseV1> segmentation;
        std::optional<MemoryWriteResult> memory_write;
        std::optional<contracts::MemoryRetrievalResponse> memories;
        std::optional<contracts::ObservationFeaturesV1> features;
        std::vector<contracts::ObservedPattern> patterns;
        std::optional<PredictionAssessment> prediction;
        std::optional<contracts::SalienceAssessmentV1> salience;
        std::optional<contracts::WorkspaceAssessment> workspace;
        std::optional<contracts::HypothesisData> hypothesis;
        std::optional<contracts::MetacognitivePortAssessment> metacognition;
        std::optional<SelfConstraintSnapshot> self_model;
        std::optional<CognitiveDecision> decision;
    };

    static std::string make_cycle_id(const std::string& event_id) {
        return digest::uuid5(
            COGNITIVE_CYCLE_NAMESPACE,
            event_id + ":" + contracts::kCognitiveCyclePolicyId);
    }

    void process_loop() {
        while (true) {
            contracts::CognitiveCycleInputV1 input;
            {
                std::unique_lock lock(queue_mutex_);
                queue_ready_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) break;
                input = std::move(queue_.front());
                queue_.pop_front();
                ++in_flight_;
            }
            log_state(input.event_id, CycleState::processing, "");
            try {
                process_input(input);
            } catch (const std::exception& error) {
                append_fatal(input, error.what());
            } catch (...) {
                append_fatal(input, "unknown_fatal_error");
            }
            {
                std::lock_guard lock(queue_mutex_);
                --in_flight_;
                if (queue_.empty() && in_flight_ == 0) idle_.notify_all();
            }
        }
        std::lock_guard lock(queue_mutex_);
        if (queue_.empty() && in_flight_ == 0) idle_.notify_all();
    }

    template <typename T, typename Callable>
    contracts::PortResult<T> invoke_stage(
        contracts::CognitiveCycleResultV1& cycle,
        std::string stage_id,
        std::string operation,
        Callable&& callable) {
        std::stop_source source;
        contracts::PortInvocationContextV1 context;
        context.correlation_id = cycle.correlation_id;
        context.deadline = std::chrono::steady_clock::now() + config_.stage_timeout;
        context.replay_mode = cycle.replay_mode;
        context.stop_token = source.get_token();
        const auto started = std::chrono::steady_clock::now();
        std::jthread timer([deadline = context.deadline, &source](std::stop_token token) {
            while (!token.stop_requested() &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if (!token.stop_requested()) source.request_stop();
        });

        contracts::PortResult<T> result;
        try {
            result = std::forward<Callable>(callable)(context);
        } catch (const std::exception& error) {
            result = contracts::PortResult<T>::failed(
                operation, "adapter_delegation_error", error.what());
        } catch (...) {
            result = contracts::PortResult<T>::failed(
                operation, "unknown_adapter_delegation_error",
                "non-standard exception");
        }
        const auto finished = std::chrono::steady_clock::now();
        const bool timed_out = finished >= context.deadline ||
            (source.stop_requested() && result.error &&
             result.error->code == "cancelled");
        timer.request_stop();

        contracts::CognitiveCycleStageV1 stage;
        stage.stage_id = std::move(stage_id);
        stage.operation = std::move(operation);
        stage.duration_microseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(finished - started)
                .count());
        if (timed_out) {
            const bool cooperative = result.error && result.error->code == "cancelled";
            const std::string timeout_code = cooperative
                ? "coordinator_timeout"
                : "timeout_uncooperative";
            stage.status = contracts::CycleStageStatusV1::timed_out;
            stage.error = contracts::PortError{
                "1.0", stage.operation, timeout_code,
                "stage exceeded its cooperative deadline", true};
            cycle.degradation_reasons.push_back(stage.stage_id + ":timeout");
            result = contracts::PortResult<T>::failed(
                stage.operation, timeout_code,
                "stage exceeded its cooperative deadline", true);
        } else if (!result.success) {
            stage.status = result.error && result.error->code == "cancelled"
                ? contracts::CycleStageStatusV1::cancelled
                : contracts::CycleStageStatusV1::failed;
            stage.error = result.error ? result.error : std::optional<contracts::PortError>{
                contracts::PortError{"1.0", stage.operation,
                                     "adapter_delegation_error",
                                     "port returned an invalid failure", false}};
            cycle.degradation_reasons.push_back(
                stage.stage_id + ":" + stage.error->code);
        } else {
            stage.status = contracts::CycleStageStatusV1::succeeded;
        }
        cycle.stages.push_back(std::move(stage));
        return result;
    }

    template <typename T, typename Predicate>
    bool accept_stage_value(contracts::CognitiveCycleResultV1& cycle,
                            const contracts::PortResult<T>& result,
                            Predicate&& predicate) {
        if (!result.success || !result.value) return false;
        if (std::forward<Predicate>(predicate)(*result.value)) return true;
        auto& stage = cycle.stages.back();
        stage.status = contracts::CycleStageStatusV1::failed;
        stage.error = contracts::PortError{
            "1.0", stage.operation, "contract_violation",
            "port returned a value that violates its output contract", false};
        cycle.degradation_reasons.push_back(stage.stage_id + ":contract_violation");
        return false;
    }

    void skip_stage(contracts::CognitiveCycleResultV1& cycle,
                    std::string stage_id,
                    std::string operation,
                    contracts::CycleStageStatusV1 status,
                    bool degraded) {
        contracts::CognitiveCycleStageV1 stage;
        stage.stage_id = std::move(stage_id);
        stage.operation = std::move(operation);
        stage.status = status;
        cycle.stages.push_back(stage);
        if (degraded) {
            cycle.degradation_reasons.push_back(
                stage.stage_id + ":" + contracts::cycle_stage_status_string(status));
        }
    }

    void process_input(const contracts::CognitiveCycleInputV1& input) {
        contracts::CognitiveCycleResultV1 cycle;
        cycle.cycle_id = make_cycle_id(input.event_id);
        cycle.correlation_id = input.correlation_id;
        cycle.input_event_id = input.event_id;
        cycle.replay_mode = input.replay_mode;
        CycleValues values;

        process_episode(input, cycle, values);
        process_memory(input, cycle, values);
        process_features(input, cycle, values);
        process_pattern(input, cycle, values);
        process_prediction(input, cycle, values);
        process_salience(input, cycle, values);
        process_workspace(input, cycle, values);
        process_hypothesis(input, cycle, values);
        process_metacognition(input, cycle, values);
        process_self_model(cycle, values);
        process_decision(input, cycle, values);

        cycle.decision = values.decision;
        cycle.state = cycle.degradation_reasons.empty()
            ? contracts::CycleStateV1::completed
            : contracts::CycleStateV1::degraded;
        if (!cycle.valid()) throw std::logic_error("invalid cognitive cycle result");
        append_result(cycle);
        log_state(input.event_id,
                  cycle.state == contracts::CycleStateV1::completed
                      ? CycleState::completed
                      : CycleState::degraded,
                  join_reasons(cycle.degradation_reasons));
        if (!input.replay_mode) publish(cycle);
    }

    void process_episode(const contracts::CognitiveCycleInputV1& input,
                         contracts::CognitiveCycleResultV1& cycle,
                         CycleValues& values) {
        const auto port = registry_.resolve<IEpisodeBoundaryPort>("episode_boundary");
        if (!port) {
            skip_stage(cycle, "episode", "episode_boundary.segment_observation",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::EpisodeObservationRequest request;
        request.event_id = input.event_id;
        request.session_id = input.session_id;
        request.occurred_at = input.occurred_at;
        request.epoch_seconds = input.epoch_seconds;
        request.application = input.application;
        request.document = input.document;
        request.modality = input.modality;
        const auto result = invoke_stage<contracts::EpisodeSegmentationResponseV1>(
            cycle, "episode", "episode_boundary.segment_observation",
            [&](const auto& context) { return port->segment_observation(request, context); });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.segmentation = *result.value;
            cycle.stages.back().evidence_refs = {result.value->update.episode_id};
        }
    }

    void process_memory(const contracts::CognitiveCycleInputV1& input,
                        contracts::CognitiveCycleResultV1& cycle,
                        CycleValues& values) {
        if (values.segmentation && values.segmentation->materialized_episode) {
            const auto port = registry_.resolve<IMemoryWritePort>("memory_write");
            if (!port) {
                skip_stage(cycle, "memory_write", "memory.store_episode",
                           contracts::CycleStageStatusV1::skipped_unavailable, true);
            } else {
                contracts::EpisodeWriteRequest request;
                request.episode = *values.segmentation->materialized_episode;
                request.start_epoch = *values.segmentation->start_epoch;
                request.end_epoch = *values.segmentation->end_epoch;
                const auto result = invoke_stage<MemoryWriteResult>(
                    cycle, "memory_write", "memory.store_episode",
                    [&](const auto& context) {
                        return port->store_episode_context(request, context);
                    });
                if (accept_stage_value(cycle, result, [](const auto& value) {
                        return value.success && !value.memory_id.empty();
                    })) {
                    values.memory_write = *result.value;
                    cycle.stages.back().evidence_refs = {result.value->memory_id};
                }
            }
        } else {
            skip_stage(cycle, "memory_write", "memory.store_episode",
                       contracts::CycleStageStatusV1::skipped_absent_input, false);
        }

        const auto retrieval = registry_.resolve<IMemoryRetrievalPort>("memory_retrieval");
        if (!retrieval) {
            skip_stage(cycle, "memory_retrieval", "memory.retrieve_memory",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::MemoryRetrievalRequest request;
        request.session_id = input.session_id;
        if (input.application) request.applications.push_back(*input.application);
        if (input.document) request.documents.push_back(*input.document);
        request.modalities = {input.modality};
        request.end_epoch = input.epoch_seconds;
        request.limit = 10;
        const auto result = invoke_stage<contracts::MemoryRetrievalResponse>(
            cycle, "memory_retrieval", "memory.retrieve_memory",
            [&](const auto& context) {
                return retrieval->retrieve_memory_context(request, context);
            });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.memories = *result.value;
            for (const auto& item : result.value->items) {
                cycle.stages.back().evidence_refs.push_back(item.memory_id);
            }
        }
    }

    void process_features(const contracts::CognitiveCycleInputV1& input,
                          contracts::CognitiveCycleResultV1& cycle,
                          CycleValues& values) {
        if (!input.numeric_features.empty()) {
            contracts::ObservationFeaturesV1 features;
            features.features = input.numeric_features;
            features.evidence_refs = {input.event_id};
            values.features = features;
            contracts::CognitiveCycleStageV1 stage;
            stage.stage_id = "features";
            stage.operation = "cycle.input.numeric_features";
            stage.status = contracts::CycleStageStatusV1::succeeded;
            stage.evidence_refs = {input.event_id};
            cycle.stages.push_back(std::move(stage));
            return;
        }
        const auto port = registry_.resolve<IObservationFeaturePort>(
            "observation_features");
        if (!port) {
            skip_stage(cycle, "features", "observation_features.extract",
                       contracts::CycleStageStatusV1::skipped_unavailable, false);
            return;
        }
        const auto result = invoke_stage<contracts::ObservationFeaturesV1>(
            cycle, "features", "observation_features.extract",
            [&](const auto& context) { return port->extract_features(input, context); });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.features = *result.value;
            cycle.stages.back().evidence_refs = result.value->evidence_refs;
        }
    }

    void process_pattern(const contracts::CognitiveCycleInputV1& input,
                         contracts::CognitiveCycleResultV1& cycle,
                         CycleValues& values) {
        if (!values.features) {
            skip_stage(cycle, "pattern", "pattern_learning.observe",
                       contracts::CycleStageStatusV1::skipped_absent_input, false);
            return;
        }
        const auto port = registry_.resolve<IPatternLearningPort>("pattern_learning");
        if (!port) {
            skip_stage(cycle, "pattern", "pattern_learning.observe",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::PatternLearningObservation observation;
        observation.features = values.features->features;
        observation.observation_ref = input.event_id;
        observation.occurred_epoch = input.epoch_seconds;
        const auto result = invoke_stage<contracts::ObservedPattern>(
            cycle, "pattern", "pattern_learning.observe",
            [&](const auto& context) { return port->observe_context(observation, context); });
        if (accept_stage_value(cycle, result, [](const auto& value) {
                return value.schema_version == "1.0" && !value.pattern_id.empty() &&
                       !value.created_by.empty();
            })) {
            values.patterns.push_back(*result.value);
            cycle.stages.back().evidence_refs = {result.value->pattern_id};
        }
    }

    void process_prediction(const contracts::CognitiveCycleInputV1& input,
                            contracts::CognitiveCycleResultV1& cycle,
                            CycleValues& values) {
        const auto port = registry_.resolve<IPredictionPort>("prediction");
        if (!port) {
            skip_stage(cycle, "prediction", "prediction.predict",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        std::vector<std::string> context_values{input.event_type};
        for (const auto& pattern : values.patterns) {
            context_values.push_back(pattern.pattern_id);
        }
        const auto result = invoke_stage<PredictionAssessment>(
            cycle, "prediction", "prediction.predict",
            [&](const auto& context) {
                return port->predict_context(
                    context_values, input.occurred_at, {input.event_type}, context);
            });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.prediction = *result.value;
            cycle.stages.back().evidence_refs = {result.value->prediction_id};
        }
    }

    void process_salience(const contracts::CognitiveCycleInputV1& input,
                          contracts::CognitiveCycleResultV1& cycle,
                          CycleValues& values) {
        if (!input.salience_signals.empty()) {
            contracts::SalienceAssessmentV1 salience;
            salience.signals = input.salience_signals;
            salience.evidence_refs = {input.event_id};
            values.salience = salience;
            contracts::CognitiveCycleStageV1 stage;
            stage.stage_id = "salience";
            stage.operation = "cycle.input.salience_signals";
            stage.status = contracts::CycleStageStatusV1::succeeded;
            stage.evidence_refs = {input.event_id};
            cycle.stages.push_back(std::move(stage));
            return;
        }
        const auto port = registry_.resolve<ISalienceAssessmentPort>("salience");
        if (!port) {
            skip_stage(cycle, "salience", "salience.assess",
                       contracts::CycleStageStatusV1::skipped_unavailable, false);
            return;
        }
        contracts::SalienceAssessmentRequestV1 request;
        request.input = input;
        request.prediction = values.prediction;
        for (const auto& pattern : values.patterns) request.pattern_ids.push_back(pattern.pattern_id);
        if (values.memories) {
            for (const auto& memory : values.memories->items) {
                request.memory_refs.push_back(memory.memory_id);
            }
        }
        const auto result = invoke_stage<contracts::SalienceAssessmentV1>(
            cycle, "salience", "salience.assess",
            [&](const auto& context) { return port->assess_salience(request, context); });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.salience = *result.value;
            cycle.stages.back().evidence_refs = result.value->evidence_refs;
        }
    }

    void process_workspace(const contracts::CognitiveCycleInputV1& input,
                           contracts::CognitiveCycleResultV1& cycle,
                           CycleValues& values) {
        if (!values.salience) {
            skip_stage(cycle, "workspace", "workspace.select_candidate",
                       contracts::CycleStageStatusV1::skipped_absent_input, false);
            return;
        }
        const auto port = registry_.resolve<IWorkspaceSelectionPort>("workspace");
        if (!port) {
            skip_stage(cycle, "workspace", "workspace.select_candidate",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::WorkspaceSelectionRequest request;
        request.candidate_id = input.event_id;
        request.session_id = input.session_id;
        request.source_kind = "canonical_event";
        request.source_refs = {input.event_id};
        request.observed_at = input.occurred_at;
        request.observed_epoch = input.epoch_seconds;
        request.content = input.content;
        request.content["event_type"] = input.event_type;
        request.content["source"] = input.source;
        request.salience_signals = values.salience->signals;
        const auto result = invoke_stage<contracts::WorkspaceAssessment>(
            cycle, "workspace", "workspace.select_candidate",
            [&](const auto& context) {
                return port->select_candidate_context(request, context);
            });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.workspace = *result.value;
            cycle.stages.back().evidence_refs = {result.value->snapshot_id};
        }
    }

    void process_hypothesis(const contracts::CognitiveCycleInputV1& input,
                            contracts::CognitiveCycleResultV1& cycle,
                            CycleValues& values) {
        const auto port = registry_.resolve<IHypothesisFormationPort>(
            "hypothesis_formation");
        if (!port) {
            skip_stage(cycle, "hypothesis", "hypothesis.form",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::HypothesisFormationRequestV1 request;
        request.input = input;
        request.prediction = values.prediction;
        request.patterns = values.patterns;
        request.workspace = values.workspace;
        if (values.memories) {
            for (const auto& memory : values.memories->items) {
                request.memory_refs.push_back(memory.memory_id);
            }
        }
        const auto result = invoke_stage<contracts::HypothesisFormationResultV1>(
            cycle, "hypothesis", "hypothesis.form",
            [&](const auto& context) { return port->form_hypothesis(request, context); });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            cycle.stages.back().evidence_refs = result.value->evidence_refs;
            if (result.value->hypothesis) values.hypothesis = *result.value->hypothesis;
        }
    }

    void process_metacognition(const contracts::CognitiveCycleInputV1& input,
                               contracts::CognitiveCycleResultV1& cycle,
                               CycleValues& values) {
        if (!values.hypothesis) {
            skip_stage(cycle, "metacognition", "metacognition.evaluate_hypothesis",
                       contracts::CycleStageStatusV1::skipped_absent_input, false);
            return;
        }
        const auto port = registry_.resolve<IMetacognitionPort>("metacognition");
        if (!port) {
            skip_stage(cycle, "metacognition", "metacognition.evaluate_hypothesis",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::MetacognitionRequest request;
        request.hypothesis = *values.hypothesis;
        request.evaluated_at = input.occurred_at;
        if (values.workspace) request.workspace_snapshot_id = values.workspace->snapshot_id;
        const auto result = invoke_stage<contracts::MetacognitivePortAssessment>(
            cycle, "metacognition", "metacognition.evaluate_hypothesis",
            [&](const auto& context) {
                return port->evaluate_hypothesis_context(request, context);
            });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.metacognition = *result.value;
            cycle.stages.back().evidence_refs = {result.value->assessment_id};
        }
    }

    void process_self_model(contracts::CognitiveCycleResultV1& cycle,
                            CycleValues& values) {
        const auto port = registry_.resolve<ISelfModelQueryPort>("self_model");
        if (!port) {
            skip_stage(cycle, "self_model", "self_model.query_constraints",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        const auto result = invoke_stage<SelfConstraintSnapshot>(
            cycle, "self_model", "self_model.query_constraints",
            [&](const auto& context) { return port->query_constraints_context(context); });
        if (accept_stage_value(cycle, result,
                               [](const auto& value) { return value.valid(); })) {
            values.self_model = *result.value;
            cycle.stages.back().evidence_refs = {result.value->model_id};
        }
    }

    void process_decision(const contracts::CognitiveCycleInputV1& input,
                          contracts::CognitiveCycleResultV1& cycle,
                          CycleValues& values) {
        if (input.replay_mode || !values.hypothesis ||
            !values.hypothesis->expected_information_gain) {
            skip_stage(cycle, "decision", "decision.decide_evidence",
                       contracts::CycleStageStatusV1::skipped_absent_input, false);
            return;
        }
        const auto port = registry_.resolve<ICognitiveDecisionPort>("decision");
        if (!port) {
            skip_stage(cycle, "decision", "decision.decide_evidence",
                       contracts::CycleStageStatusV1::skipped_unavailable, true);
            return;
        }
        contracts::DecisionRequest request;
        request.event_id = input.event_id;
        request.event_type = input.event_type;
        request.occurred_at = input.occurred_at;
        request.hypothesis_id = values.hypothesis->hypothesis_id;
        request.confidence = values.hypothesis->confidence;
        request.information_gain = *values.hypothesis->expected_information_gain;
        request.evidence_ids = values.hypothesis->supporting_refs;
        if (std::find(request.evidence_ids.begin(), request.evidence_ids.end(),
                      input.event_id) == request.evidence_ids.end()) {
            request.evidence_ids.push_back(input.event_id);
        }
        request.reason = values.hypothesis->statement;
        if (values.workspace) request.workspace_snapshot_id = values.workspace->snapshot_id;
        const auto result = invoke_stage<CognitiveDecision>(
            cycle, "decision", "decision.decide_evidence",
            [&](const auto& context) {
                return port->decide_evidence_context(request, context);
            });
        if (accept_stage_value(cycle, result, [](const auto& value) {
                return value.success && !value.intent.empty();
            })) {
            values.decision = *result.value;
            if (!result.value->target_action.empty()) {
                cycle.stages.back().evidence_refs = {result.value->target_action};
            }
        }
    }

    static std::string join_reasons(const std::vector<std::string>& reasons) {
        std::string output;
        for (const auto& reason : reasons) {
            if (!output.empty()) output += "; ";
            output += reason;
        }
        return output;
    }

    void append_discarded(const contracts::CognitiveCycleInputV1& input,
                          const std::string& cycle_id,
                          const std::string& reason) {
        std::lock_guard lock(queue_mutex_);
        append_discarded_locked(input, cycle_id, reason);
    }

    void append_discarded_locked(const contracts::CognitiveCycleInputV1& input,
                                 const std::string& cycle_id,
                                 const std::string& reason) {
        contracts::CognitiveCycleResultV1 result;
        result.cycle_id = cycle_id;
        result.correlation_id = input.correlation_id.empty()
            ? (input.event_id.empty() ? "invalid" : input.event_id)
            : input.correlation_id;
        result.input_event_id = input.event_id.empty() ? "invalid" : input.event_id;
        result.state = contracts::CycleStateV1::discarded;
        result.discard_reason = reason;
        result.replay_mode = input.replay_mode;
        append_result(result);
        log_state(result.input_event_id, CycleState::discarded, reason);
    }

    void append_fatal(const contracts::CognitiveCycleInputV1& input,
                      const std::string& reason) {
        contracts::CognitiveCycleResultV1 result;
        result.cycle_id = make_cycle_id(input.event_id);
        result.correlation_id = input.correlation_id;
        result.input_event_id = input.event_id;
        result.state = contracts::CycleStateV1::failed;
        result.degradation_reasons = {reason};
        result.replay_mode = input.replay_mode;
        append_result(result);
        log_state(input.event_id, CycleState::failed, reason);
    }

    void append_result(const contracts::CognitiveCycleResultV1& result) {
        std::lock_guard lock(result_mutex_);
        results_.push_back(result);
    }

    void publish(const contracts::CognitiveCycleResultV1& result) {
        std::function<void(const CanonicalEvent&)> publisher;
        {
            std::lock_guard lock(publisher_mutex_);
            publisher = publisher_;
        }
        if (!publisher) return;
        CanonicalEvent event;
        event.event_id = result.cycle_id;
        event.source = COGNITIVE_COORDINATOR_SOURCE;
        event.event_type = "cognitive.cycle.result";
        event.payload = result.to_json();
        try {
            publisher(event);
        } catch (...) {
            // The cycle result is already committed locally. Publishing is best effort
            // and must never create a second terminal result for the same input.
        }
    }

    void log_state(const std::string& event_id, CycleState state,
                   const std::string& reason) {
        std::lock_guard lock(log_mutex_);
        logs_.push_back({event_id, state, reason});
    }

    const CapabilityRegistry& registry_;
    CognitiveCoordinatorConfig config_;

    mutable std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::condition_variable idle_;
    std::deque<contracts::CognitiveCycleInputV1> queue_;
    std::unordered_set<std::string> seen_event_ids_;
    std::size_t in_flight_{0};
    bool started_{false};
    bool stopping_{false};
    std::thread worker_;

    mutable std::mutex result_mutex_;
    std::vector<contracts::CognitiveCycleResultV1> results_;
    mutable std::mutex log_mutex_;
    std::vector<CycleStatusLog> logs_;
    mutable std::mutex publisher_mutex_;
    std::function<void(const CanonicalEvent&)> publisher_;

    mutable std::mutex context_mutex_;
    CognitiveCycleContext last_context_;
};

}  // namespace eu_digital
