#pragma once

#include "core/contracts/cognitive_decision.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/episode_update.hpp"
#include "core/contracts/pattern_learning.hpp"
#include "core/contracts/port_result.hpp"
#include "core/contracts/prediction_assessment.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stop_token>
#include <string>
#include <vector>

namespace eu_digital::contracts {

inline constexpr char kCognitiveCycleSchemaVersion[] = "1.0";
inline constexpr char kCognitiveCyclePolicyId[] = "bounded_ports_v1";

enum class CycleStateV1 {
    queued,
    processing,
    degraded,
    completed,
    failed,
    discarded
};

enum class CycleStageStatusV1 {
    succeeded,
    failed,
    skipped_absent_input,
    skipped_unavailable,
    cancelled,
    timed_out
};

inline std::string cycle_state_string(CycleStateV1 value) {
    switch (value) {
    case CycleStateV1::queued: return "queued";
    case CycleStateV1::processing: return "processing";
    case CycleStateV1::degraded: return "degraded";
    case CycleStateV1::completed: return "completed";
    case CycleStateV1::failed: return "failed";
    case CycleStateV1::discarded: return "discarded";
    }
    return "failed";
}

inline std::string cycle_stage_status_string(CycleStageStatusV1 value) {
    switch (value) {
    case CycleStageStatusV1::succeeded: return "succeeded";
    case CycleStageStatusV1::failed: return "failed";
    case CycleStageStatusV1::skipped_absent_input: return "skipped_absent_input";
    case CycleStageStatusV1::skipped_unavailable: return "skipped_unavailable";
    case CycleStageStatusV1::cancelled: return "cancelled";
    case CycleStageStatusV1::timed_out: return "timed_out";
    }
    return "failed";
}

inline std::string cycle_json_string(const std::string& value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20U) {
                constexpr char digits[] = "0123456789abcdef";
                output << "\\u00" << digits[(character >> 4U) & 0x0fU]
                       << digits[character & 0x0fU];
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

inline std::string cycle_json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << cycle_json_string(values[index]);
    }
    output << ']';
    return output.str();
}

struct PortInvocationContextV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::string correlation_id;
    std::chrono::steady_clock::time_point deadline{};
    bool replay_mode{false};
    std::stop_token stop_token;

    bool valid() const {
        return schema_version == kCognitiveCycleSchemaVersion &&
               !correlation_id.empty() &&
               deadline != std::chrono::steady_clock::time_point{};
    }

    bool stop_requested() const {
        return stop_token.stop_requested() ||
               (deadline != std::chrono::steady_clock::time_point{} &&
                std::chrono::steady_clock::now() >= deadline);
    }
};

struct CognitiveCycleInputV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::string correlation_id;
    std::string event_id;
    std::string source;
    std::string event_type;
    std::string session_id;
    std::string occurred_at;
    double epoch_seconds{0.0};
    std::string modality;
    std::optional<std::string> application;
    std::optional<std::string> document;
    std::map<std::string, std::string> content;
    std::map<std::string, double> numeric_features;
    std::map<std::string, double> salience_signals;
    std::vector<std::string> causation_chain;
    std::string time_basis{"source_occurred"};
    bool replay_mode{false};

    bool valid() const {
        static const std::set<std::string> time_bases{
            "source_occurred", "received_fallback"};
        const auto valid_numbers = [](const auto& values, bool probability) {
            return std::all_of(values.begin(), values.end(), [&](const auto& item) {
                return !item.first.empty() && std::isfinite(item.second) &&
                       (!probability || (item.second >= 0.0 && item.second <= 1.0));
            });
        };
        return schema_version == kCognitiveCycleSchemaVersion &&
               !correlation_id.empty() && !event_id.empty() && !source.empty() &&
               !event_type.empty() && !session_id.empty() && !occurred_at.empty() &&
               std::isfinite(epoch_seconds) && !modality.empty() &&
               (!application || !application->empty()) &&
               (!document || !document->empty()) &&
               valid_numbers(numeric_features, false) &&
               valid_numbers(salience_signals, true) &&
               valid_references(causation_chain) && time_bases.contains(time_basis);
    }
};

struct EpisodeSegmentationResponseV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    EpisodeUpdate update;
    std::optional<EpisodeData> materialized_episode;
    std::optional<double> start_epoch;
    std::optional<double> end_epoch;

    bool valid() const {
        const bool materialized = materialized_episode.has_value();
        return schema_version == kCognitiveCycleSchemaVersion && update.valid() &&
               (!materialized_episode || materialized_episode->valid()) &&
               (materialized == start_epoch.has_value()) &&
               (materialized == end_epoch.has_value()) &&
               (!start_epoch || std::isfinite(*start_epoch)) &&
               (!end_epoch || std::isfinite(*end_epoch)) &&
               (!start_epoch || !end_epoch || *end_epoch >= *start_epoch);
    }
};

struct ObservationFeaturesV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::map<std::string, double> features;
    std::vector<std::string> evidence_refs;

    bool valid() const {
        return schema_version == kCognitiveCycleSchemaVersion && !features.empty() &&
               std::all_of(features.begin(), features.end(), [](const auto& item) {
                   return !item.first.empty() && std::isfinite(item.second);
               }) && valid_references(evidence_refs, false);
    }
};

struct SalienceAssessmentV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::map<std::string, double> signals;
    std::vector<std::string> evidence_refs;

    bool valid() const {
        return schema_version == kCognitiveCycleSchemaVersion && !signals.empty() &&
               std::all_of(signals.begin(), signals.end(), [](const auto& item) {
                   return !item.first.empty() && valid_probability(item.second);
               }) && valid_references(evidence_refs, false);
    }
};

struct SalienceAssessmentRequestV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    CognitiveCycleInputV1 input;
    std::optional<PredictionAssessment> prediction;
    std::vector<std::string> pattern_ids;
    std::vector<std::string> memory_refs;

    bool valid() const {
        return schema_version == kCognitiveCycleSchemaVersion && input.valid() &&
               valid_references(pattern_ids) && valid_references(memory_refs) &&
               (!prediction || prediction->valid());
    }
};

struct HypothesisFormationRequestV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    CognitiveCycleInputV1 input;
    std::optional<PredictionAssessment> prediction;
    std::vector<ObservedPattern> patterns;
    std::optional<WorkspaceAssessment> workspace;
    std::vector<std::string> memory_refs;

    bool valid() const {
        return schema_version == kCognitiveCycleSchemaVersion && input.valid() &&
               (!prediction || prediction->valid()) &&
               (!workspace || workspace->valid()) && valid_references(memory_refs) &&
               std::all_of(patterns.begin(), patterns.end(), [](const auto& pattern) {
                   return !pattern.pattern_id.empty() && pattern.schema_version == "1.0";
               });
    }
};

struct HypothesisFormationResultV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::string outcome{"insufficient_evidence"};
    std::optional<HypothesisData> hypothesis;
    std::vector<std::string> evidence_refs;

    bool valid() const {
        const bool formed = outcome == "formed";
        return schema_version == kCognitiveCycleSchemaVersion &&
               (formed || outcome == "insufficient_evidence") &&
               (formed == hypothesis.has_value()) &&
               (!hypothesis || hypothesis->valid()) &&
               valid_references(evidence_refs);
    }
};

struct CognitiveCycleStageV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::string stage_id;
    std::string operation;
    CycleStageStatusV1 status{CycleStageStatusV1::skipped_unavailable};
    std::vector<std::string> evidence_refs;
    std::optional<PortError> error;
    std::uint64_t duration_microseconds{0};

    bool valid() const {
        const bool needs_error = status == CycleStageStatusV1::failed ||
                                 status == CycleStageStatusV1::cancelled ||
                                 status == CycleStageStatusV1::timed_out;
        return schema_version == kCognitiveCycleSchemaVersion &&
               !stage_id.empty() && !operation.empty() &&
               (needs_error == error.has_value()) &&
               (!error || error->valid()) && valid_references(evidence_refs);
    }

    std::string to_json() const {
        std::ostringstream output;
        output << "{\"duration_microseconds\":" << duration_microseconds
               << ",\"error\":";
        if (error) {
            output << "{\"code\":" << cycle_json_string(error->code)
                   << ",\"message\":" << cycle_json_string(error->message)
                   << ",\"operation\":" << cycle_json_string(error->operation)
                   << ",\"retryable\":" << (error->retryable ? "true" : "false")
                   << ",\"schema_version\":\"1.0\"}";
        } else {
            output << "null";
        }
        output << ",\"evidence_refs\":" << cycle_json_array(evidence_refs)
               << ",\"operation\":" << cycle_json_string(operation)
               << ",\"schema_version\":\"1.0\""
               << ",\"stage_id\":" << cycle_json_string(stage_id)
               << ",\"status\":" << cycle_json_string(cycle_stage_status_string(status))
               << '}';
        return output.str();
    }
};

struct CognitiveCycleResultV1 {
    std::string schema_version{kCognitiveCycleSchemaVersion};
    std::string cycle_id;
    std::string correlation_id;
    std::string input_event_id;
    CycleStateV1 state{CycleStateV1::failed};
    std::string policy_id{kCognitiveCyclePolicyId};
    std::vector<CognitiveCycleStageV1> stages;
    std::optional<CognitiveDecision> decision;
    std::vector<std::string> degradation_reasons;
    std::optional<std::string> discard_reason;
    bool replay_mode{false};

    bool valid() const {
        const bool final_state = state == CycleStateV1::completed ||
                                 state == CycleStateV1::degraded ||
                                 state == CycleStateV1::failed ||
                                 state == CycleStateV1::discarded;
        const bool degraded_valid = state != CycleStateV1::degraded ||
                                    !degradation_reasons.empty();
        const bool discard_valid = state == CycleStateV1::discarded
            ? discard_reason && !discard_reason->empty()
            : !discard_reason.has_value();
        return schema_version == kCognitiveCycleSchemaVersion && final_state &&
               !cycle_id.empty() && !correlation_id.empty() &&
               !input_event_id.empty() && policy_id == kCognitiveCyclePolicyId &&
               degraded_valid && discard_valid &&
               valid_references(degradation_reasons) &&
               std::all_of(stages.begin(), stages.end(),
                           [](const auto& stage) { return stage.valid(); });
    }

    std::string to_json() const {
        std::ostringstream output;
        output << "{\"correlation_id\":" << cycle_json_string(correlation_id)
               << ",\"cycle_id\":" << cycle_json_string(cycle_id)
               << ",\"decision\":";
        if (decision) {
            output << "{\"error_message\":" << cycle_json_string(decision->error_message)
                   << ",\"intent\":" << cycle_json_string(decision->intent)
                   << ",\"reason\":" << cycle_json_string(decision->reason)
                   << ",\"success\":" << (decision->success ? "true" : "false")
                   << ",\"target_action\":" << cycle_json_string(decision->target_action)
                   << '}';
        } else {
            output << "null";
        }
        output << ",\"degradation_reasons\":" << cycle_json_array(degradation_reasons)
               << ",\"discard_reason\":"
               << (discard_reason ? cycle_json_string(*discard_reason) : "null")
               << ",\"input_event_id\":" << cycle_json_string(input_event_id)
               << ",\"policy_id\":" << cycle_json_string(policy_id)
               << ",\"replay_mode\":" << (replay_mode ? "true" : "false")
               << ",\"schema_version\":\"1.0\",\"stages\":[";
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (index) output << ',';
            output << stages[index].to_json();
        }
        output << "],\"state\":" << cycle_json_string(cycle_state_string(state)) << '}';
        return output.str();
    }
};

}  // namespace eu_digital::contracts
