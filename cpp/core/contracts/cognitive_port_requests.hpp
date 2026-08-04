#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace eu_digital::contracts {

inline constexpr char kCognitivePortRequestSchemaVersion[] = "1.0";

inline bool valid_probability(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

inline bool valid_references(const std::vector<std::string>& values,
                             bool allow_empty = true) {
    return (allow_empty || !values.empty()) &&
           std::all_of(values.begin(), values.end(),
                       [](const auto& value) { return !value.empty(); });
}

struct EpisodeObservationRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::string event_id;
    std::string session_id;
    std::string occurred_at;
    double epoch_seconds{0.0};
    std::optional<std::string> application;
    std::optional<std::string> document;
    std::string modality;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               !event_id.empty() && !session_id.empty() && !occurred_at.empty() &&
               std::isfinite(epoch_seconds) && !modality.empty() &&
               (!application || !application->empty()) &&
               (!document || !document->empty());
    }
};

struct EpisodeData {
    std::string schema_version{"1.0"};
    std::string episode_id;
    std::string session_id;
    std::string start_at;
    std::string end_at;
    std::vector<std::string> event_ids;
    std::vector<std::string> applications;
    std::vector<std::string> documents;
    std::vector<std::string> people;
    std::vector<std::string> topics;
    std::vector<std::string> modalities;
    std::vector<std::string> boundary_reasons;
    std::optional<std::string> embedding_ref;
    std::optional<std::string> summary;
    std::vector<std::string> hypotheses;
    double coherence{1.0};
    double confidence{1.0};
    std::string created_by;

    bool valid() const {
        return schema_version == "1.0" && !episode_id.empty() &&
               !session_id.empty() && !start_at.empty() && !end_at.empty() &&
               valid_references(event_ids, false) &&
               valid_references(applications) && valid_references(documents) &&
               valid_references(people) && valid_references(topics) &&
               valid_references(modalities) && valid_references(boundary_reasons) &&
               valid_references(hypotheses) &&
               (!embedding_ref || !embedding_ref->empty()) &&
               (!summary || !summary->empty()) && valid_probability(coherence) &&
               valid_probability(confidence) && !created_by.empty();
    }
};

struct EpisodeWriteRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    EpisodeData episode;
    double start_epoch{0.0};
    double end_epoch{0.0};
    std::optional<std::vector<double>> embedding;

    bool valid() const {
        const bool embedding_valid = !embedding ||
            (!embedding->empty() &&
             std::all_of(embedding->begin(), embedding->end(),
                         [](double value) { return std::isfinite(value); }));
        return schema_version == kCognitivePortRequestSchemaVersion &&
               episode.valid() && std::isfinite(start_epoch) &&
               std::isfinite(end_epoch) && end_epoch >= start_epoch && embedding_valid;
    }
};

struct MemoryRetrievalRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::optional<std::string> session_id;
    std::vector<std::string> applications;
    std::vector<std::string> documents;
    std::vector<std::string> modalities;
    std::optional<double> start_epoch;
    std::optional<double> end_epoch;
    std::optional<std::vector<double>> embedding;
    std::size_t limit{10};

    bool valid() const {
        const bool embedding_valid = !embedding ||
            (!embedding->empty() &&
             std::all_of(embedding->begin(), embedding->end(),
                         [](double value) { return std::isfinite(value); }));
        return schema_version == kCognitivePortRequestSchemaVersion && limit > 0 &&
               (!session_id || !session_id->empty()) && valid_references(applications) &&
               valid_references(documents) && valid_references(modalities) &&
               (!start_epoch || std::isfinite(*start_epoch)) &&
               (!end_epoch || std::isfinite(*end_epoch)) &&
               (!start_epoch || !end_epoch || *end_epoch >= *start_epoch) &&
               embedding_valid;
    }
};

struct MemoryRetrievalItem {
    std::string memory_id;
    double relevance{0.0};
    std::string session_id;
    std::vector<std::string> event_ids;
    std::vector<std::string> applications;
    std::vector<std::string> documents;
    std::vector<std::string> modalities;
    std::vector<std::string> reason_codes;

    bool valid() const {
        return !memory_id.empty() && std::isfinite(relevance) && relevance >= 0.0 &&
               !session_id.empty() && valid_references(event_ids, false) &&
               valid_references(applications) && valid_references(documents) &&
               valid_references(modalities) && valid_references(reason_codes);
    }
};

struct MemoryRetrievalResponse {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::vector<MemoryRetrievalItem> items;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               std::all_of(items.begin(), items.end(),
                           [](const auto& item) { return item.valid(); });
    }
};

struct WorkspaceSelectionRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::string candidate_id;
    std::string session_id;
    std::string source_kind;
    std::vector<std::string> source_refs;
    std::string observed_at;
    double observed_epoch{0.0};
    std::map<std::string, std::string> content;
    std::map<std::string, double> salience_signals;

    bool valid() const {
        static const std::set<std::string> source_kinds{
            "canonical_event", "episode", "pattern", "internal"};
        return schema_version == kCognitivePortRequestSchemaVersion &&
               !candidate_id.empty() && !session_id.empty() &&
               source_kinds.contains(source_kind) &&
               valid_references(source_refs, false) && !observed_at.empty() &&
               std::isfinite(observed_epoch) && !salience_signals.empty() &&
               std::all_of(salience_signals.begin(), salience_signals.end(),
                           [](const auto& item) {
                               return !item.first.empty() && valid_probability(item.second);
                           });
    }
};

struct WorkspaceAssessment {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::string snapshot_id;
    std::string workspace_id;
    std::string session_id;
    std::string created_at;
    int capacity{0};
    std::string policy_id;
    std::string config_fingerprint;
    double selection_churn{0.0};
    std::vector<std::string> active_candidate_ids;
    std::vector<std::string> expired_candidate_ids;
    std::vector<std::string> discarded_candidate_ids;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               !snapshot_id.empty() && !workspace_id.empty() && !session_id.empty() &&
               !created_at.empty() && capacity > 0 && !policy_id.empty() &&
               !config_fingerprint.empty() && valid_probability(selection_churn) &&
               valid_references(active_candidate_ids) &&
               valid_references(expired_candidate_ids) &&
               valid_references(discarded_candidate_ids);
    }
};

struct HypothesisData {
    std::string schema_version{"1.0"};
    std::string hypothesis_id;
    std::string kind;
    std::string statement;
    std::string status{"proposed"};
    double confidence{0.5};
    std::vector<std::string> supporting_refs;
    std::vector<std::string> opposing_refs;
    std::vector<std::string> alternatives;
    std::string created_at;
    std::string updated_at;
    std::optional<std::string> verification_question;
    std::optional<double> expected_information_gain;
    std::string provenance_module;
    std::optional<std::string> model_version;

    bool valid() const {
        static const std::set<std::string> kinds{
            "observed_pattern", "causal", "predictive", "contextual", "capability"};
        static const std::set<std::string> statuses{
            "proposed", "confirmed", "rejected", "superseded"};
        return schema_version == "1.0" && !hypothesis_id.empty() &&
               kinds.contains(kind) && !statement.empty() && statuses.contains(status) &&
               valid_probability(confidence) && valid_references(supporting_refs) &&
               valid_references(opposing_refs) && valid_references(alternatives) &&
               !created_at.empty() && !updated_at.empty() &&
               (!verification_question || !verification_question->empty()) &&
               (!expected_information_gain || valid_probability(*expected_information_gain)) &&
               !provenance_module.empty() && (!model_version || !model_version->empty());
    }
};

struct MetacognitionRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    HypothesisData hypothesis;
    std::string evaluated_at;
    std::optional<std::string> workspace_snapshot_id;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               hypothesis.valid() && !evaluated_at.empty() &&
               (!workspace_snapshot_id || !workspace_snapshot_id->empty());
    }
};

struct MetacognitivePortAssessment {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::string assessment_id;
    std::string hypothesis_id;
    std::string evaluated_at;
    double curiosity_score{0.0};
    bool requires_exploration{false};
    std::string focus_area;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               !assessment_id.empty() && !hypothesis_id.empty() &&
               !evaluated_at.empty() && valid_probability(curiosity_score) &&
               !focus_area.empty();
    }
};

struct DecisionRequest {
    std::string schema_version{kCognitivePortRequestSchemaVersion};
    std::string event_id;
    std::string event_type;
    std::string occurred_at;
    std::string hypothesis_id;
    double confidence{0.0};
    double information_gain{0.0};
    std::vector<std::string> evidence_ids;
    std::string reason;
    std::optional<std::string> workspace_snapshot_id;

    bool valid() const {
        return schema_version == kCognitivePortRequestSchemaVersion &&
               !event_id.empty() && !event_type.empty() && !occurred_at.empty() &&
               !hypothesis_id.empty() && valid_probability(confidence) &&
               std::isfinite(information_gain) && information_gain >= 0.0 &&
               valid_references(evidence_ids, false) && !reason.empty() &&
               (!workspace_snapshot_id || !workspace_snapshot_id->empty());
    }
};

}  // namespace eu_digital::contracts
