#include "core/episode_segmenter.hpp"
#include "core/episodic_memory.hpp"
#include "core/functional_self_model.hpp"
#include "core/global_workspace.hpp"
#include "core/metacognition_curiosity.hpp"
#include "core/pattern_learner.hpp"
#include "core/promotion_registry.hpp"
#include "core/runtime_host.hpp"
#include "core/world_model.hpp"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

namespace {

namespace runtime_detail = eu_digital::runtime_detail;
using runtime_detail::JsonValue;

std::string required_string(const JsonValue::Object& object, const std::string& key, const std::string& path) {
    return runtime_detail::string(runtime_detail::required(object, key, path), path + "." + key);
}

std::optional<std::string> optional_string(const JsonValue::Object& object, const std::string& key, const std::string& path) {
    const auto found = object.find(key);
    if (found == object.end() || runtime_detail::is_null(found->second)) return std::nullopt;
    return runtime_detail::string(found->second, path + "." + key);
}

const JsonValue::Object* optional_object(const JsonValue::Object& object, const std::string& key, const std::string& path) {
    const auto found = object.find(key);
    if (found == object.end() || runtime_detail::is_null(found->second)) return nullptr;
    return &runtime_detail::object(found->second, path + "." + key);
}

std::optional<std::string> first_string(const JsonValue::Object& object,
                                        const std::initializer_list<const char*>& keys,
                                        const std::string& path) {
    for (const auto* key : keys) {
        if (const auto value = optional_string(object, key, path)) return value;
    }
    return std::nullopt;
}

std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
    const unsigned day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const unsigned day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

double parse_timestamp(const std::string& value) {
    if (value.size() < 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T') {
        throw std::runtime_error("invalid ISO-8601 timestamp: " + value);
    }
    const auto number = [&](std::size_t offset, std::size_t length) {
        return std::stoi(value.substr(offset, length));
    };
    const int year = number(0, 4);
    const unsigned month = static_cast<unsigned>(number(5, 2));
    const unsigned day = static_cast<unsigned>(number(8, 2));
    const int hour = number(11, 2);
    const int minute = number(14, 2);
    const int second = number(17, 2);
    const auto zone_start = value.find_first_of("Z+-", 19);
    if (zone_start == std::string::npos) throw std::runtime_error("timestamp must include timezone: " + value);
    double fraction = 0.0;
    if (zone_start > 19) fraction = std::stod("0" + value.substr(19, zone_start - 19));
    int offset_seconds = 0;
    if (value[zone_start] != 'Z') {
        if (value.size() < zone_start + 6 || value[zone_start + 3] != ':') throw std::runtime_error("invalid timestamp timezone: " + value);
        const int sign = value[zone_start] == '-' ? -1 : 1;
        offset_seconds = sign * (number(zone_start + 1, 2) * 3600 + number(zone_start + 4, 2) * 60);
    }
    return static_cast<double>(days_from_civil(year, month, day) * 86400LL + hour * 3600 + minute * 60 + second - offset_seconds) + fraction;
}

std::optional<std::string> context_value(const JsonValue::Object& payload, const std::initializer_list<const char*>& payload_keys,
                                          const std::initializer_list<const char*>& context_keys, const std::string& path) {
    if (const auto value = first_string(payload, payload_keys, path + ".payload")) return value;
    if (const auto* context = optional_object(payload, "context", path + ".payload")) {
        return first_string(*context, context_keys, path + ".payload.context");
    }
    return std::nullopt;
}

eu_digital::EpisodeSegmentEvent parse_event(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::EpisodeSegmentEvent event;
    event.event_id = required_string(object, "event_id", path);
    event.session_id = required_string(object, "session_id", path);
    event.occurred_at = required_string(object, "occurred_at", path);
    event.epoch_seconds = parse_timestamp(event.occurred_at);
    const auto source = optional_string(object, "source", path).value_or("");
    const auto event_type = optional_string(object, "event_type", path).value_or("");
    const auto payload_value = object.find("payload");
    const JsonValue::Object empty_payload;
    const auto& payload = payload_value == object.end() || runtime_detail::is_null(payload_value->second)
        ? empty_payload : runtime_detail::object(payload_value->second, path + ".payload");
    event.application = context_value(payload, {"application", "app"}, {"process_name", "application", "app"}, path);
    event.document = context_value(payload, {"document", "document_uri"}, {"document_uri", "document"}, path);
    std::string lower_source = source;
    std::string lower_type = event_type;
    for (auto& character : lower_source) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    for (auto& character : lower_type) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    if (lower_source.find("ocr") != std::string::npos || lower_type.find("ocr") != std::string::npos) event.modality = "ocr";
    else if (lower_source == "input" || lower_source == "user" || lower_type.find("key") != std::string::npos || lower_type.find("mouse") != std::string::npos || lower_type.find("input") != std::string::npos) event.modality = "input";
    else if (!lower_source.empty()) event.modality = lower_source;
    return event;
}

std::string json_string(const std::string& value) { return "\"" + runtime_detail::escape_json(value) + "\""; }

std::string json_array(const std::vector<std::string>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << json_string(values[index]);
    }
    output << ']';
    return output.str();
}

std::vector<std::string> string_array(const JsonValue& value, const std::string& path) {
    const auto& values = runtime_detail::array(value, path);
    std::vector<std::string> result;
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        result.push_back(runtime_detail::string(values[index], path + "[" + std::to_string(index) + "]"));
    }
    return result;
}

std::vector<double> number_array(const JsonValue& value, const std::string& path) {
    const auto& values = runtime_detail::array(value, path);
    std::vector<double> result;
    result.reserve(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto* number = std::get_if<runtime_detail::JsonNumber>(&values[index].value);
        if (!number) throw std::runtime_error(path + " must contain numbers");
        result.push_back(std::stod(number->text));
    }
    return result;
}

double number_value(const JsonValue& value, const std::string& path) {
    const auto* number = std::get_if<runtime_detail::JsonNumber>(&value.value);
    if (!number) throw std::runtime_error(path + " must be a number");
    return std::stod(number->text);
}

std::string json_number(double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

std::string json_optional_string(const std::optional<std::string>& value) {
    return value ? json_string(*value) : "null";
}

std::string json_optional_number(const std::optional<double>& value) {
    return value ? json_number(*value) : "null";
}

std::string json_optional_boolean(const std::optional<bool>& value) {
    if (!value) return "null";
    return *value ? "true" : "false";
}

std::string json_value_raw(const JsonValue& value) {
    if (runtime_detail::is_null(value)) return "null";
    if (const auto found = std::get_if<bool>(&value.value)) return *found ? "true" : "false";
    if (const auto found = std::get_if<runtime_detail::JsonNumber>(&value.value)) return found->text;
    if (const auto found = std::get_if<std::string>(&value.value)) return json_string(*found);
    if (const auto found = std::get_if<JsonValue::Array>(&value.value)) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < found->size(); ++index) {
            if (index) output << ',';
            output << json_value_raw((*found)[index]);
        }
        output << ']';
        return output.str();
    }
    const auto& object = std::get<JsonValue::Object>(value.value);
    std::ostringstream output;
    output << '{';
    std::size_t index = 0;
    for (const auto& [key, nested] : object) {
        if (index++) output << ',';
        output << json_string(key) << ':' << json_value_raw(nested);
    }
    output << '}';
    return output.str();
}

std::map<std::string, std::string> json_object_raw(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    std::map<std::string, std::string> result;
    for (const auto& [key, nested] : object) result.emplace(key, json_value_raw(nested));
    return result;
}

std::string json_raw_object(const std::map<std::string, std::string>& values) {
    std::ostringstream output;
    output << '{';
    std::size_t index = 0;
    for (const auto& [key, raw] : values) {
        if (index++) output << ',';
        output << json_string(key) << ':' << raw;
    }
    output << '}';
    return output.str();
}

std::string json_number_canonical(double value) {
    return eu_digital::workspace_python_float(value);
}

eu_digital::WorkspaceConfig parse_workspace_config(const JsonValue::Object& object, const std::string& path) {
    eu_digital::WorkspaceConfig config;
    if (const auto* value = optional_object(object, "config", path)) {
        if (const auto found = value->find("capacity"); found != value->end()) config.capacity = static_cast<int>(runtime_detail::unsigned_number(found->second, path + ".config.capacity"));
        if (const auto found = value->find("ttl_seconds"); found != value->end()) config.ttl_seconds = number_value(found->second, path + ".config.ttl_seconds");
        if (const auto found = value->find("max_candidates"); found != value->end()) config.max_candidates = static_cast<int>(runtime_detail::unsigned_number(found->second, path + ".config.max_candidates"));
        if (const auto found = value->find("selection_policy"); found != value->end()) config.selection_policy = runtime_detail::string(found->second, path + ".config.selection_policy");
        if (const auto found = value->find("weights"); found != value->end()) {
            const auto& weights = runtime_detail::object(found->second, path + ".config.weights");
            for (const auto& [name, weight] : weights) config.weights[name] = number_value(weight, path + ".config.weights." + name);
        }
        if (const auto found = value->find("enabled_factors"); found != value->end()) {
            config.enabled_factors.clear();
            for (const auto& name : string_array(found->second, path + ".config.enabled_factors")) config.enabled_factors.insert(name);
        }
    }
    config.validate();
    return config;
}

eu_digital::WorkspaceCandidate parse_workspace_candidate(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::WorkspaceCandidate candidate;
    candidate.candidate_id = required_string(object, "candidate_id", path);
    candidate.schema_version = optional_string(object, "schema_version", path).value_or(eu_digital::WORKSPACE_SCHEMA_VERSION);
    candidate.session_id = required_string(object, "session_id", path);
    candidate.source_kind = required_string(object, "source_kind", path);
    candidate.source_refs = string_array(runtime_detail::required(object, "source_refs", path), path + ".source_refs");
    candidate.observed_at = required_string(object, "observed_at", path);
    candidate.content = json_object_raw(runtime_detail::required(object, "content", path), path + ".content");
    const auto& signals = runtime_detail::object(runtime_detail::required(object, "salience_signals", path), path + ".salience_signals");
    for (const auto& [name, signal] : signals) candidate.salience_signals[name] = number_value(signal, path + ".salience_signals." + name);
    candidate.validate();
    return candidate;
}

std::string serialize_workspace_salience(const eu_digital::WorkspaceSalience& salience) {
    std::ostringstream output;
    output << "{\"missing_factors\":" << json_array(salience.missing_factors)
           << ",\"observed_factors\":{";
    std::size_t index = 0;
    for (const auto& [name, value] : salience.observed_factors) {
        if (index++) output << ',';
        output << json_string(name) << ':' << json_number_canonical(value);
    }
    output << "},\"policy_id\":" << json_string(salience.policy_id)
           << ",\"score\":" << json_number_canonical(salience.score) << '}';
    return output.str();
}

std::string serialize_workspace_decision(const eu_digital::WorkspaceDecision& decision) {
    std::ostringstream output;
    output << "{\"candidate_id\":" << json_string(decision.candidate_id)
           << ",\"rank\":" << (decision.rank ? std::to_string(*decision.rank) : "null")
           << ",\"reason_codes\":" << json_array(decision.reason_codes)
           << ",\"score\":" << (decision.score ? json_number_canonical(*decision.score) : "null")
           << ",\"selected\":" << (decision.selected ? "true" : "false") << '}';
    return output.str();
}

std::string serialize_workspace_item(const eu_digital::WorkspaceItem& item) {
    std::ostringstream output;
    output << "{\"admitted_at\":" << json_string(item.admitted_at)
           << ",\"candidate_id\":" << json_string(item.candidate_id)
           << ",\"content\":" << json_raw_object(item.content)
           << ",\"expires_at\":" << json_string(item.expires_at)
           << ",\"observed_at\":" << json_string(item.observed_at)
           << ",\"salience\":" << serialize_workspace_salience(item.salience)
           << ",\"schema_version\":" << json_string(item.schema_version)
           << ",\"selection\":{\"rank\":" << item.rank
           << ",\"reasons\":" << json_array(item.selection_reasons)
           << ",\"selected_at\":" << json_string(item.selected_at)
           << ",\"snapshot_id\":" << json_string(item.snapshot_id) << "}"
           << ",\"session_id\":" << json_string(item.session_id)
           << ",\"source_kind\":" << json_string(item.source_kind)
           << ",\"source_refs\":" << json_array(item.source_refs)
           << ",\"workspace_id\":" << json_string(item.workspace_id)
           << ",\"workspace_item_id\":" << json_string(item.workspace_item_id)
           << '}';
    return output.str();
}

std::string serialize_workspace_snapshot(const eu_digital::WorkspaceSnapshot& snapshot) {
    std::ostringstream output;
    output << "{\"active_items\":[";
    for (std::size_t index = 0; index < snapshot.active_items.size(); ++index) {
        if (index) output << ',';
        output << serialize_workspace_item(snapshot.active_items[index]);
    }
    output << "],\"capacity\":" << snapshot.capacity
           << ",\"config_fingerprint\":" << json_string(snapshot.config_fingerprint)
           << ",\"created_at\":" << json_string(snapshot.created_at)
           << ",\"decisions\":[";
    for (std::size_t index = 0; index < snapshot.decisions.size(); ++index) {
        if (index) output << ',';
        output << serialize_workspace_decision(snapshot.decisions[index]);
    }
    output << "],\"discarded_candidate_ids\":" << json_array(snapshot.discarded_candidate_ids)
           << ",\"expired_candidate_ids\":" << json_array(snapshot.expired_candidate_ids)
           << ",\"policy_id\":" << json_string(snapshot.policy_id)
           << ",\"schema_version\":" << json_string(snapshot.schema_version)
           << ",\"selection_churn\":" << json_number_canonical(snapshot.selection_churn)
           << ",\"session_id\":" << json_string(snapshot.session_id)
           << ",\"snapshot_id\":" << json_string(snapshot.snapshot_id)
           << ",\"workspace_id\":" << json_string(snapshot.workspace_id) << '}';
    return output.str();
}

std::string serialize_workspace_broadcast(const eu_digital::WorkspaceBroadcast& broadcast) {
    std::ostringstream output;
    output << "{\"broadcast_id\":" << json_string(broadcast.broadcast_id)
           << ",\"emitted_at\":" << json_string(broadcast.emitted_at)
           << ",\"schema_version\":" << json_string(broadcast.schema_version)
           << ",\"session_id\":" << json_string(broadcast.session_id)
           << ",\"snapshot\":" << serialize_workspace_snapshot(broadcast.snapshot)
           << ",\"workspace_id\":" << json_string(broadcast.workspace_id) << '}';
    return output.str();
}

eu_digital::MemoryEpisode parse_memory_episode(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::MemoryEpisode episode;
    episode.episode_id = required_string(object, "episode_id", path);
    episode.schema_version = required_string(object, "schema_version", path);
    episode.session_id = required_string(object, "session_id", path);
    episode.start_at = required_string(object, "start_at", path);
    episode.end_at = required_string(object, "end_at", path);
    episode.start_epoch = parse_timestamp(episode.start_at);
    episode.end_epoch = parse_timestamp(episode.end_at);
    episode.event_ids = string_array(runtime_detail::required(object, "event_ids", path), path + ".event_ids");
    const auto& context = runtime_detail::object(runtime_detail::required(object, "context_summary", path), path + ".context_summary");
    episode.applications = string_array(runtime_detail::required(context, "applications", path + ".context_summary"), path + ".context_summary.applications");
    episode.documents = string_array(runtime_detail::required(context, "documents", path + ".context_summary"), path + ".context_summary.documents");
    episode.people = string_array(runtime_detail::required(context, "people", path + ".context_summary"), path + ".context_summary.people");
    episode.topics = string_array(runtime_detail::required(context, "topics", path + ".context_summary"), path + ".context_summary.topics");
    episode.modalities = string_array(runtime_detail::required(context, "modalities", path + ".context_summary"), path + ".context_summary.modalities");
    episode.boundary_reasons = string_array(runtime_detail::required(object, "boundary_reasons", path), path + ".boundary_reasons");
    episode.embedding_ref = optional_string(object, "embedding_ref", path);
    episode.summary = optional_string(object, "summary", path);
    episode.hypotheses = string_array(runtime_detail::required(object, "hypotheses", path), path + ".hypotheses");
    const auto& quality = runtime_detail::object(runtime_detail::required(object, "quality", path), path + ".quality");
    episode.coherence = number_value(runtime_detail::required(quality, "coherence", path + ".quality"), path + ".quality.coherence");
    episode.confidence = number_value(runtime_detail::required(quality, "confidence", path + ".quality"), path + ".quality.confidence");
    episode.created_by = required_string(object, "created_by", path);
    return episode;
}

std::string serialize_memory_episode(const eu_digital::MemoryEpisode& episode) {
    std::ostringstream output;
    output << "{\"boundary_reasons\":" << json_array(episode.boundary_reasons)
           << ",\"context_summary\":{\"applications\":" << json_array(episode.applications)
           << ",\"documents\":" << json_array(episode.documents)
           << ",\"modalities\":" << json_array(episode.modalities)
           << ",\"people\":" << json_array(episode.people)
           << ",\"topics\":" << json_array(episode.topics) << "},\"created_by\":" << json_string(episode.created_by)
           << ",\"embedding_ref\":" << json_optional_string(episode.embedding_ref)
           << ",\"end_at\":" << json_string(episode.end_at)
           << ",\"episode_id\":" << json_string(episode.episode_id)
           << ",\"event_ids\":" << json_array(episode.event_ids)
           << ",\"hypotheses\":" << json_array(episode.hypotheses)
           << ",\"quality\":{\"coherence\":" << json_number(episode.coherence)
           << ",\"confidence\":" << json_number(episode.confidence) << "},\"schema_version\":" << json_string(episode.schema_version)
           << ",\"session_id\":" << json_string(episode.session_id)
           << ",\"start_at\":" << json_string(episode.start_at)
           << ",\"summary\":" << json_optional_string(episode.summary) << '}';
    return output.str();
}

std::string json_reasons(const std::vector<std::string>& reasons) { return json_array(reasons); }

std::string json_join_reasons(const std::vector<std::string>& reasons) {
    std::ostringstream output;
    for (std::size_t index = 0; index < reasons.size(); ++index) {
        if (index) output << ", ";
        output << reasons[index];
    }
    return output.str();
}

std::string serialize_memory_result(const std::vector<std::string>& store_results,
                                    const std::vector<std::string>& consolidated,
                                    std::size_t size,
                                    const std::vector<eu_digital::MemoryRetrievalResult>& retrieval,
                                    const std::vector<eu_digital::MemoryRelation>& relations) {
    std::ostringstream output;
    output << "{\"consolidated\":" << json_array(consolidated) << ",\"relations\":[";
    for (std::size_t index = 0; index < relations.size(); ++index) {
        if (index) output << ',';
        const auto& relation = relations[index];
        output << "{\"episode_a\":" << json_string(relation.episode_a)
               << ",\"episode_b\":" << json_string(relation.episode_b)
               << ",\"provenance\":{\"event_ids\":" << json_array(relation.event_ids) << "},\"reason_codes\":"
               << json_reasons(relation.reason_codes) << ",\"score\":" << json_number(relation.score) << '}';
    }
    output << "],\"retrieval\":[";
    for (std::size_t index = 0; index < retrieval.size(); ++index) {
        if (index) output << ',';
        const auto& item = retrieval[index];
        output << "{\"episode\":" << serialize_memory_episode(item.episode)
               << ",\"explanation\":" << json_string("episode " + item.episode.episode_id + " retrieved because " + json_join_reasons(item.reason_codes))
               << ",\"provenance\":{\"created_by\":" << json_string(item.episode.created_by)
               << ",\"episode_id\":" << json_string(item.episode.episode_id)
               << ",\"event_ids\":" << json_array(item.episode.event_ids)
               << ",\"schema_version\":" << json_string(item.episode.schema_version) << "},\"reason_codes\":"
               << json_reasons(item.reason_codes) << ",\"score\":" << json_number(item.score) << '}';
    }
    output << "],\"schema_version\":\"1.0\",\"size\":" << size << ",\"store_results\":" << json_array(store_results) << "}\n";
    return output.str();
}

eu_digital::PatternObservation parse_pattern_observation(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::PatternObservation observation;
    const auto& features = runtime_detail::object(runtime_detail::required(object, "features", path), path + ".features");
    for (const auto& [key, feature] : features) observation.features[key] = number_value(feature, path + ".features." + key);
    observation.observation_ref = required_string(object, "observation_ref", path);
    observation.occurred_epoch = parse_timestamp(required_string(object, "occurred_at", path));
    return observation;
}

std::string serialize_pattern(const eu_digital::PatternRecord& pattern) {
    std::ostringstream output;
    output << "{\"centroid\":{";
    std::size_t index = 0;
    for (const auto& [key, value] : pattern.centroid) {
        if (index++) output << ',';
        output << json_string(key) << ':' << json_number(value);
    }
    output << "},\"confidence\":" << json_number(pattern.confidence)
           << ",\"created_by\":" << json_string(pattern.created_by)
           << ",\"drift_reason\":" << json_optional_string(pattern.drift_reason)
           << ",\"feedback\":{\"negative\":" << pattern.negative_feedback
           << ",\"positive\":" << pattern.positive_feedback
           << ",\"references\":" << json_array(pattern.feedback_references) << "}"
           << ",\"observation_refs\":" << json_array(pattern.observation_refs)
           << ",\"parent_pattern_id\":" << json_optional_string(pattern.parent_pattern_id)
           << ",\"pattern_id\":" << json_string(pattern.pattern_id)
           << ",\"recency\":" << json_number(pattern.recency)
           << ",\"schema_version\":" << json_string(pattern.schema_version)
           << ",\"stability\":" << json_number(pattern.stability)
           << ",\"status\":" << json_string(pattern.status)
           << ",\"support\":" << pattern.support
           << ",\"version\":" << pattern.version << '}';
    return output.str();
}

std::string serialize_pattern_snapshot(const eu_digital::PatternLearner& learner) {
    std::ostringstream output;
    output << "{\"patterns\":[";
    const auto records = learner.records();
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (index) output << ',';
        output << serialize_pattern(records[index]);
    }
    output << "],\"schema_version\":\"1.0\",\"stream_id\":" << json_string(learner.stream_id()) << '}';
    return output.str();
}

std::string serialize_pattern_metrics(const eu_digital::PatternLearner& learner) {
    std::ostringstream output;
    output << "{\"ablation\":\"set distance_threshold to zero and disable cross-feature clustering\",\"baseline_id\":\"online_exact_threshold_v1\",\"clusters\":[";
    const auto records = learner.records();
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (index) output << ',';
        const auto& pattern = records[index];
        const int feedback_count = pattern.positive_feedback + pattern.negative_feedback;
        output << "{\"confidence\":" << json_number(pattern.confidence)
               << ",\"false_discovery_rate\":" << json_number(feedback_count == 0 ? 0.0 : static_cast<double>(pattern.negative_feedback) / feedback_count)
               << ",\"pattern_id\":" << json_string(pattern.pattern_id)
               << ",\"recency\":" << json_number(pattern.recency)
               << ",\"stability\":" << json_number(pattern.stability)
               << ",\"status\":" << json_string(pattern.status)
               << ",\"support\":" << pattern.support << ",\"version\":" << pattern.version << '}';
    }
    output << "],\"falsification\":\"incremental clustering does not beat the exact-key baseline\",\"registered\":true}";
    return output.str();
}

int run_pattern_learning() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        eu_digital::PatternConfig config;
        if (const auto* config_object = optional_object(object, "config", "fixture")) {
            if (const auto found = config_object->find("min_support"); found != config_object->end()) config.min_support = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.min_support"));
            if (const auto found = config_object->find("distance_threshold"); found != config_object->end()) config.distance_threshold = number_value(found->second, "fixture.config.distance_threshold");
            if (const auto found = config_object->find("promotion_confidence"); found != config_object->end()) config.promotion_confidence = number_value(found->second, "fixture.config.promotion_confidence");
        }
        const std::string stream_id = required_string(object, "stream_id", "fixture");
        eu_digital::PatternLearner learner(config, stream_id);
        std::vector<eu_digital::PatternRecord> observations;
        std::vector<eu_digital::PatternRecord> feedback_results;
        std::string last_pattern_id;
        const auto& operations = runtime_detail::array(runtime_detail::required(object, "operations", "fixture"), "fixture.operations");
        for (std::size_t index = 0; index < operations.size(); ++index) {
            const auto& operation = runtime_detail::object(operations[index], "fixture.operations[" + std::to_string(index) + "]");
            const auto type = required_string(operation, "type", "fixture.operations[" + std::to_string(index) + "]");
            if (type == "observe") {
                const auto record = learner.observe(parse_pattern_observation(runtime_detail::required(operation, "observation", "fixture.operations[" + std::to_string(index) + "]"), "fixture.operations[" + std::to_string(index) + "].observation"));
                last_pattern_id = record.pattern_id;
                observations.push_back(record);
            } else if (type == "feedback") {
                const auto target = required_string(operation, "target", "fixture.operations[" + std::to_string(index) + "]");
                const auto pattern_id = target == "last_observation" ? last_pattern_id : target;
                const auto record = learner.feedback(pattern_id, runtime_detail::boolean(runtime_detail::required(operation, "positive", "fixture.operations[" + std::to_string(index) + "]"), "fixture.operations[" + std::to_string(index) + "].positive"), required_string(operation, "reference", "fixture.operations[" + std::to_string(index) + "]"));
                feedback_results.push_back(record);
            } else {
                throw std::runtime_error("unsupported pattern operation: " + type);
            }
        }
        std::ostringstream output;
        output << "{\"feedback\":[";
        for (std::size_t index = 0; index < feedback_results.size(); ++index) { if (index) output << ','; output << serialize_pattern(feedback_results[index]); }
        output << "],\"metrics\":" << serialize_pattern_metrics(learner) << ",\"observations\":[";
        for (std::size_t index = 0; index < observations.size(); ++index) { if (index) output << ','; output << serialize_pattern(observations[index]); }
        output << "],\"snapshot\":" << serialize_pattern_snapshot(learner) << ",\"schema_version\":\"1.0\"}\n";
        std::cout << output.str();
    }
    return std::cout.good() ? 0 : 1;
}

std::string serialize_world_distribution(const std::map<std::string, double>& distribution) {
    std::ostringstream output;
    output << '{';
    std::size_t index = 0;
    for (const auto& [state, probability] : distribution) {
        if (index++) output << ',';
        output << json_string(state) << ':' << json_number(probability);
    }
    output << '}';
    return output.str();
}

std::string serialize_world_prediction(const eu_digital::WorldPrediction& prediction) {
    std::ostringstream output;
    output << "{\"confidence\":" << json_number(prediction.confidence)
           << ",\"context\":" << json_array(prediction.context)
           << ",\"created_by\":" << json_string(prediction.created_by)
           << ",\"drift_id\":" << json_optional_string(prediction.drift_id)
           << ",\"log_loss\":" << json_optional_number(prediction.log_loss)
           << ",\"model_id\":" << json_string(prediction.model_id)
           << ",\"observed_state\":" << json_optional_string(prediction.observed_state)
           << ",\"predicted_at\":" << json_string(prediction.predicted_at)
           << ",\"predicted_distribution\":" << serialize_world_distribution(prediction.predicted_distribution)
           << ",\"prediction_id\":" << json_string(prediction.prediction_id)
           << ",\"salience_contribution\":" << json_number(prediction.salience_contribution)
           << ",\"schema_version\":" << json_string(prediction.schema_version)
           << ",\"stream_id\":" << json_string(prediction.stream_id)
           << ",\"top_k\":" << prediction.top_k
           << ",\"top_k_hit\":" << json_optional_boolean(prediction.top_k_hit) << '}';
    return output.str();
}

std::string serialize_world_error(const eu_digital::WorldPrediction& prediction, const std::string& observed_at) {
    if (!prediction.observed_state || !prediction.log_loss || !prediction.top_k_hit) {
        throw std::runtime_error("cannot serialize an unscored prediction error");
    }
    std::ostringstream output;
    output << "{\"confidence\":" << json_number(prediction.confidence)
           << ",\"drift_id\":" << json_optional_string(prediction.drift_id)
           << ",\"log_loss\":" << json_number(*prediction.log_loss)
           << ",\"model_id\":" << json_string(prediction.model_id)
           << ",\"observed_at\":" << json_string(observed_at)
           << ",\"observed_state\":" << json_string(*prediction.observed_state)
           << ",\"prediction_id\":" << json_string(prediction.prediction_id)
           << ",\"salience_contribution\":" << json_number(prediction.salience_contribution)
           << ",\"schema_version\":" << json_string(prediction.schema_version)
           << ",\"top_k_hit\":" << (*prediction.top_k_hit ? "true" : "false") << '}';
    return output.str();
}

std::string serialize_world_drift(const eu_digital::WorldDriftSignal& drift) {
    std::ostringstream output;
    output << "{\"confidence_after\":" << json_number(drift.confidence_after)
           << ",\"confidence_before\":" << json_number(drift.confidence_before)
           << ",\"detected_at\":" << json_string(drift.detected_at)
           << ",\"drift_id\":" << json_string(drift.drift_id)
           << ",\"model_id\":" << json_string(drift.model_id)
           << ",\"reason\":" << json_string(drift.reason)
           << ",\"relearning_started\":" << (drift.relearning_started ? "true" : "false")
           << ",\"rolling_log_loss\":" << json_number(drift.rolling_log_loss)
           << ",\"schema_version\":" << json_string(drift.schema_version)
           << ",\"stream_id\":" << json_string(drift.stream_id)
           << ",\"threshold\":" << json_number(drift.threshold)
           << ",\"trigger_prediction_id\":" << json_string(drift.trigger_prediction_id) << '}';
    return output.str();
}

std::string serialize_world_metrics(const eu_digital::WorldModel& model) {
    std::ostringstream output;
    output << "{\"ablation\":\"replace incremental context with frequency_baseline_v0\",\"confidence\":"
           << json_number(model.confidence())
           << ",\"drift_count\":" << model.drifts().size()
           << ",\"falsification\":\"prediction does not beat frequency on the frozen holdout\""
           << ",\"mean_log_loss\":" << json_optional_number(model.mean_log_loss())
           << ",\"model_id\":" << json_string(eu_digital::world_model_policy_id(model.policy()))
           << ",\"prediction_count\":" << model.prediction_count()
           << ",\"promoted_pattern_count\":" << model.promoted_pattern_count()
           << ",\"relearning_observations\":" << model.relearning_observations()
           << ",\"relearning_started\":" << (model.relearning_started() ? "true" : "false")
           << ",\"scored_count\":" << model.scored_count()
           << ",\"stream_id\":" << json_string(model.stream_id())
           << ",\"top_k_accuracy\":" << json_optional_number(model.top_k_accuracy()) << '}';
    return output.str();
}

int run_world_model() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        eu_digital::WorldModelConfig config;
        if (const auto* config_object = optional_object(object, "config", "fixture")) {
            if (const auto found = config_object->find("max_order"); found != config_object->end()) {
                config.max_order = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.max_order"));
            }
            if (const auto found = config_object->find("smoothing"); found != config_object->end()) {
                config.smoothing = number_value(found->second, "fixture.config.smoothing");
            }
            if (const auto found = config_object->find("drift_window"); found != config_object->end()) {
                config.drift_window = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.drift_window"));
            }
            if (const auto found = config_object->find("drift_threshold"); found != config_object->end()) {
                config.drift_threshold = number_value(found->second, "fixture.config.drift_threshold");
            }
            if (const auto found = config_object->find("top_k"); found != config_object->end()) {
                config.top_k = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.top_k"));
            }
        }
        const auto stream_id = required_string(object, "stream_id", "fixture");
        const auto policy = eu_digital::world_model_policy_from_id(
            optional_string(object, "policy", "fixture").value_or(eu_digital::PREDICTOR_POLICY_ID));
        std::vector<eu_digital::PromotedPatternInput> promoted_patterns;
        if (const auto found = object.find("patterns"); found != object.end()) {
            const auto& pattern_values = runtime_detail::array(found->second, "fixture.patterns");
            promoted_patterns.reserve(pattern_values.size());
            for (std::size_t index = 0; index < pattern_values.size(); ++index) {
                const auto path = "fixture.patterns[" + std::to_string(index) + "]";
                const auto& pattern = runtime_detail::object(pattern_values[index], path);
                eu_digital::PromotedPatternInput input;
                input.pattern_id = required_string(pattern, "pattern_id", path);
                input.status = required_string(pattern, "status", path);
                if (const auto confidence = pattern.find("confidence"); confidence != pattern.end()) {
                    input.confidence = number_value(confidence->second, path + ".confidence");
                }
                promoted_patterns.push_back(std::move(input));
            }
        }
        eu_digital::WorldModel model(config, stream_id, policy, std::move(promoted_patterns));
        std::vector<eu_digital::WorldPrediction> predictions;
        std::vector<std::string> errors;
        std::string last_prediction_id;
        const auto& operations = runtime_detail::array(runtime_detail::required(object, "operations", "fixture"), "fixture.operations");
        for (std::size_t index = 0; index < operations.size(); ++index) {
            const auto path = "fixture.operations[" + std::to_string(index) + "]";
            const auto& operation = runtime_detail::object(operations[index], path);
            const auto type = required_string(operation, "type", path);
            if (type == "observe") {
                const auto& observation = runtime_detail::object(runtime_detail::required(operation, "observation", path), path + ".observation");
                model.observe(
                    required_string(observation, "state", path + ".observation"),
                    required_string(observation, "event_ref", path + ".observation"),
                    parse_timestamp(required_string(observation, "occurred_at", path + ".observation")));
            } else if (type == "predict") {
                std::vector<std::string> context;
                if (const auto found = operation.find("context"); found != operation.end()) context = string_array(found->second, path + ".context");
                std::vector<std::string> candidate_states;
                if (const auto found = operation.find("candidate_states"); found != operation.end()) candidate_states = string_array(found->second, path + ".candidate_states");
                const auto prediction = model.predict(
                    context,
                    required_string(operation, "predicted_at", path),
                    candidate_states);
                predictions.push_back(prediction);
                last_prediction_id = prediction.prediction_id;
            } else if (type == "score") {
                const auto target = required_string(operation, "target", path);
                const auto target_id = target == "last_prediction" ? last_prediction_id : target;
                auto found = std::find_if(predictions.begin(), predictions.end(), [&](const auto& prediction) {
                    return prediction.prediction_id == target_id;
                });
                if (found == predictions.end()) throw std::runtime_error("unknown prediction target: " + target_id);
                const auto observed_at = required_string(operation, "observed_at", path);
                *found = model.score(*found, required_string(operation, "observed_state", path), observed_at);
                errors.push_back(serialize_world_error(*found, observed_at));
            } else {
                throw std::runtime_error("unsupported world model operation: " + type);
            }
        }
        std::ostringstream output;
        output << "{\"drifts\":[";
        for (std::size_t index = 0; index < model.drifts().size(); ++index) {
            if (index) output << ',';
            output << serialize_world_drift(model.drifts()[index]);
        }
        output << "],\"errors\":[";
        for (std::size_t index = 0; index < errors.size(); ++index) {
            if (index) output << ',';
            output << errors[index];
        }
        output << "],\"metrics\":" << serialize_world_metrics(model) << ",\"predictions\":[";
        for (std::size_t index = 0; index < predictions.size(); ++index) {
            if (index) output << ',';
            output << serialize_world_prediction(predictions[index]);
        }
        output << "],\"schema_version\":\"1.0\"}\n";
        std::cout << output.str();
    }
    return std::cout.good() ? 0 : 1;
}

std::string serialize_workspace_event(const eu_digital::WorkspaceBroadcast& broadcast, double emitted_epoch) {
    const auto payload = serialize_workspace_broadcast(broadcast);
    std::ostringstream output;
    output << "{\"actor_id\":null,\"context\":{\"workspace_id\":" << json_string(broadcast.workspace_id)
           << "},\"event_id\":" << json_string(broadcast.broadcast_id)
           << ",\"event_type\":\"workspace.selection.v1\",\"monotonic_ns\":"
           << std::max<std::int64_t>(0, static_cast<std::int64_t>(emitted_epoch * 1000000000.0))
           << ",\"occurred_at\":" << json_string(broadcast.emitted_at)
           << ",\"payload\":" << payload
           << ",\"privacy_class\":\"local\",\"provenance\":{\"module\":"
           << json_string(eu_digital::WORKSPACE_CREATED_BY) << ",\"snapshot_id\":"
           << json_string(broadcast.snapshot.snapshot_id)
           << "},\"quality\":{\"completeness\":1.0,\"latency_ms\":0},\"received_at\":"
           << json_string(broadcast.emitted_at)
           << ",\"schema_version\":\"1.0\",\"session_id\":" << json_string(broadcast.session_id)
           << ",\"source\":\"global_workspace\",\"tags\":[\"workspace\",\"selection\"]}";
    return output.str();
}

int run_global_workspace() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        const auto workspace_id = required_string(object, "workspace_id", "fixture");
        const auto session_id = required_string(object, "session_id", "fixture");
        auto workspace = eu_digital::GlobalWorkspace(workspace_id, session_id, parse_workspace_config(object, "fixture"));
        const auto& operations = runtime_detail::array(runtime_detail::required(object, "operations", "fixture"), "fixture.operations");
        std::vector<eu_digital::WorkspaceSnapshot> snapshots;
        std::vector<std::string> broadcasts;
        for (std::size_t index = 0; index < operations.size(); ++index) {
            const auto path = "fixture.operations[" + std::to_string(index) + "]";
            const auto& operation = runtime_detail::object(operations[index], path);
            const auto type = required_string(operation, "type", path);
            const auto now_value = optional_string(operation, "now", path);
            const auto now_epoch = parse_timestamp(now_value.value_or("1970-01-01T00:00:00+00:00"));
            const auto now = eu_digital::workspace_format_utc(now_epoch);
            if (type == "admit") {
                snapshots.push_back(workspace.admit(parse_workspace_candidate(runtime_detail::required(operation, "candidate", path), path + ".candidate"), now, now_epoch));
            } else if (type == "snapshot") {
                snapshots.push_back(workspace.snapshot(now, now_epoch));
            } else if (type == "update_priority") {
                snapshots.push_back(workspace.update_priority(required_string(operation, "candidate_id", path), number_value(runtime_detail::required(operation, "priority", path), path + ".priority"), now, now_epoch));
            } else if (type == "broadcast") {
                if (snapshots.empty()) throw std::runtime_error("workspace broadcast requires a prior snapshot");
                const auto emitted_value = optional_string(operation, "emitted_at", path).value_or(now);
                const auto emitted_epoch = parse_timestamp(emitted_value);
                const auto emitted_at = eu_digital::workspace_format_utc(emitted_epoch);
                const auto broadcast = workspace.broadcast(snapshots.back(), emitted_at, emitted_epoch);
                broadcasts.push_back(serialize_workspace_event(broadcast, emitted_epoch));
            } else {
                throw std::runtime_error("unsupported global workspace operation: " + type);
            }
        }
        std::ostringstream output;
        output << "{\"broadcasts\":[";
        for (std::size_t index = 0; index < broadcasts.size(); ++index) {
            if (index) output << ',';
            output << broadcasts[index];
        }
        output << "],\"schema_version\":\"1.0\",\"snapshots\":[";
        for (std::size_t index = 0; index < snapshots.size(); ++index) {
            if (index) output << ',';
            output << serialize_workspace_snapshot(snapshots[index]);
        }
        output << "]}\n";
        std::cout << output.str();
    }
    return std::cout.good() ? 0 : 1;
}

eu_digital::FunctionalSelfModelAssertion parse_self_model_assertion(
    const JsonValue::Object& object, const std::vector<std::string>& fallback_sources,
    const std::string& path) {
    eu_digital::FunctionalSelfModelAssertion assertion;
    assertion.assertion_id = required_string(object, "assertion_id", path);
    assertion.subject = required_string(object, "subject", path);
    assertion.predicate = required_string(object, "predicate", path);
    assertion.value = required_string(object, "value", path);
    assertion.classification = required_string(object, "classification", path);
    assertion.explanation = required_string(object, "explanation", path);
    const auto found = object.find("source_event_ids");
    assertion.source_event_ids = found == object.end()
        ? fallback_sources
        : string_array(found->second, path + ".source_event_ids");
    assertion.validate();
    return assertion;
}

eu_digital::FunctionalSelfModelEvent parse_self_model_event(
    const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::FunctionalSelfModelEvent event;
    event.event_id = required_string(object, "event_id", path);
    event.schema_version = optional_string(object, "schema_version", path)
        .value_or(eu_digital::FUNCTIONAL_SELF_MODEL_SCHEMA_VERSION);
    event.occurred_at = eu_digital::workspace_format_utc(
        parse_timestamp(required_string(object, "occurred_at", path)));
    event.kind = required_string(object, "kind", path);
    event.reason = required_string(object, "reason", path);
    event.source_event_ids = string_array(
        runtime_detail::required(object, "source_event_ids", path),
        path + ".source_event_ids");
    if (const auto* capability = optional_object(object, "capability", path)) {
        eu_digital::FunctionalSelfModelCapability parsed;
        parsed.capability_id = required_string(*capability, "capability_id", path + ".capability");
        parsed.status = required_string(*capability, "status", path + ".capability");
        parsed.explanation = required_string(*capability, "explanation", path + ".capability");
        parsed.source_event_ids = event.source_event_ids;
        event.capability = std::move(parsed);
    }
    if (const auto* assertion = optional_object(object, "assertion", path)) {
        event.assertion = parse_self_model_assertion(
            *assertion, event.source_event_ids, path + ".assertion");
    }
    event.validate();
    return event;
}

std::string serialize_self_model_assertion(
    const eu_digital::FunctionalSelfModelAssertion& assertion) {
    std::ostringstream output;
    output << "{\"assertion_id\":" << json_string(assertion.assertion_id)
           << ",\"classification\":" << json_string(assertion.classification)
           << ",\"explanation\":" << json_string(assertion.explanation)
           << ",\"predicate\":" << json_string(assertion.predicate)
           << ",\"source_event_ids\":" << json_array(assertion.source_event_ids)
           << ",\"subject\":" << json_string(assertion.subject)
           << ",\"value\":" << json_string(assertion.value) << '}';
    return output.str();
}

std::string serialize_self_model_capability(
    const eu_digital::FunctionalSelfModelCapability& capability) {
    std::ostringstream output;
    output << "{\"capability_id\":" << json_string(capability.capability_id)
           << ",\"explanation\":" << json_string(capability.explanation)
           << ",\"source_event_ids\":" << json_array(capability.source_event_ids)
           << ",\"status\":" << json_string(capability.status) << '}';
    return output.str();
}

std::string serialize_self_model_assertion_array(
    const std::vector<eu_digital::FunctionalSelfModelAssertion>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << serialize_self_model_assertion(values[index]);
    }
    output << ']';
    return output.str();
}

std::string serialize_self_model_capability_array(
    const std::vector<eu_digital::FunctionalSelfModelCapability>& values) {
    std::ostringstream output;
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index) output << ',';
        output << serialize_self_model_capability(values[index]);
    }
    output << ']';
    return output.str();
}

std::string serialize_self_model_snapshot(
    const eu_digital::FunctionalSelfModelSnapshot& snapshot) {
    std::ostringstream output;
    output << "{\"capabilities\":" << serialize_self_model_capability_array(snapshot.capabilities)
           << ",\"configuration\":" << serialize_self_model_assertion_array(snapshot.configuration)
           << ",\"facts\":" << serialize_self_model_assertion_array(snapshot.facts)
           << ",\"history_hash\":" << json_string(snapshot.history_hash)
           << ",\"hypotheses\":" << serialize_self_model_assertion_array(snapshot.hypotheses)
           << ",\"prior_snapshot_id\":" << json_optional_string(snapshot.prior_snapshot_id)
           << ",\"schema_version\":" << json_string(snapshot.schema_version)
           << ",\"snapshot_id\":" << json_string(snapshot.snapshot_id)
           << ",\"trigger_event_id\":" << json_optional_string(snapshot.trigger_event_id)
           << ",\"updated_at\":" << json_string(snapshot.updated_at)
           << ",\"version\":" << snapshot.version << '}';
    return output.str();
}

std::string serialize_self_model_decision(
    const eu_digital::FunctionalSelfModelDecision& decision) {
    std::ostringstream output;
    output << "{\"allowed\":" << (decision.allowed ? "true" : "false")
           << ",\"decision_id\":" << json_string(decision.decision_id)
           << ",\"explanation\":" << json_string(decision.explanation)
           << ",\"policy_id\":" << json_string(decision.policy_id)
           << ",\"reason_code\":" << json_string(decision.reason_code)
           << ",\"requested_capability_id\":" << json_string(decision.requested_capability_id)
           << ",\"schema_version\":" << json_string(decision.schema_version)
           << ",\"snapshot_id\":" << json_string(decision.snapshot_id) << '}';
    return output.str();
}

std::string serialize_self_model_metrics(
    const eu_digital::VersionedFunctionalSelfModel& model) {
    std::ostringstream output;
    output << "{\"ablation\":" << json_string(eu_digital::FUNCTIONAL_SELF_MODEL_ABLATION)
           << ",\"applied_event_count\":" << model.applied_event_count()
           << ",\"baseline_policy_id\":" << json_string(eu_digital::FUNCTIONAL_SELF_MODEL_BASELINE_ID)
           << ",\"falsification\":" << json_string(eu_digital::FUNCTIONAL_SELF_MODEL_FALSIFICATION)
           << ",\"history_version_count\":" << model.history_version_count()
           << ",\"hypothesis\":" << json_string(eu_digital::FUNCTIONAL_SELF_MODEL_HYPOTHESIS)
           << ",\"policy_id\":" << json_string(model.decision_policy()) << '}';
    return output.str();
}

int run_functional_self_model() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        const auto model_id = required_string(object, "model_id", "fixture");
        const auto initial_at = eu_digital::workspace_format_utc(
            parse_timestamp(required_string(object, "initial_at", "fixture")));
        const auto policy = optional_string(object, "decision_policy", "fixture")
            .value_or(eu_digital::FUNCTIONAL_SELF_MODEL_POLICY_ID);
        eu_digital::VersionedFunctionalSelfModel model(model_id, initial_at, policy);
        std::vector<eu_digital::FunctionalSelfModelDecision> decisions;
        std::vector<eu_digital::FunctionalSelfModelSnapshot> version_reads;
        const auto& operations = runtime_detail::array(
            runtime_detail::required(object, "operations", "fixture"), "fixture.operations");
        for (std::size_t index = 0; index < operations.size(); ++index) {
            const auto path = "fixture.operations[" + std::to_string(index) + "]";
            const auto& operation = runtime_detail::object(operations[index], path);
            const auto type = required_string(operation, "type", path);
            if (type == "apply") {
                model.apply(parse_self_model_event(
                    runtime_detail::required(operation, "event", path), path + ".event"));
            } else if (type == "decide") {
                decisions.push_back(model.decide(
                    required_string(operation, "requested_capability_id", path)));
            } else if (type == "version") {
                const auto version = static_cast<int>(runtime_detail::unsigned_number(
                    runtime_detail::required(operation, "version", path), path + ".version"));
                version_reads.push_back(model.version(version));
            } else {
                throw std::runtime_error("unsupported functional self-model operation: " + type);
            }
        }
        std::ostringstream output;
        output << "{\"decisions\":[";
        for (std::size_t index = 0; index < decisions.size(); ++index) {
            if (index) output << ',';
            output << serialize_self_model_decision(decisions[index]);
        }
        output << "],\"model\":{\"history\":[";
        for (std::size_t index = 0; index < model.history().size(); ++index) {
            if (index) output << ',';
            output << serialize_self_model_snapshot(model.history()[index]);
        }
        output << "],\"metrics\":" << serialize_self_model_metrics(model)
               << ",\"model_id\":" << json_string(model.model_id())
               << ",\"schema_version\":\"1.0\"},\"schema_version\":\"1.0\",\"version_reads\":[";
        for (std::size_t index = 0; index < version_reads.size(); ++index) {
            if (index) output << ',';
            output << serialize_self_model_snapshot(version_reads[index]);
        }
        output << "]}\n";
        std::cout << output.str();
    }
    return std::cout.good() ? 0 : 1;
}

eu_digital::HypothesisStatus parse_hypothesis_status(const std::string& value) {
    if (value == "proposed") return eu_digital::HypothesisStatus::proposed;
    if (value == "confirmed") return eu_digital::HypothesisStatus::confirmed;
    if (value == "rejected") return eu_digital::HypothesisStatus::rejected;
    if (value == "superseded") return eu_digital::HypothesisStatus::superseded;
    throw std::runtime_error("unsupported hypothesis status: " + value);
}

eu_digital::QuestionPolicy parse_question_policy(const std::string& value) {
    if (value == eu_digital::METACOGNITION_INFORMATION_GAIN_POLICY_ID) return eu_digital::QuestionPolicy::information_gain_v1;
    if (value == eu_digital::METACOGNITION_BASELINE_QUESTION_POLICY_ID) return eu_digital::QuestionPolicy::fixed_gain_v0;
    throw std::runtime_error("unsupported metacognition question policy: " + value);
}

eu_digital::ResponseOutcome parse_response_outcome(const std::string& value) {
    if (value == "confirmed") return eu_digital::ResponseOutcome::confirmed;
    if (value == "rejected") return eu_digital::ResponseOutcome::rejected;
    if (value == "inconclusive") return eu_digital::ResponseOutcome::inconclusive;
    throw std::runtime_error("unsupported curiosity response outcome: " + value);
}

std::optional<double> optional_number(const JsonValue::Object& object, const std::string& key, const std::string& path) {
    const auto found = object.find(key);
    if (found == object.end() || runtime_detail::is_null(found->second)) return std::nullopt;
    return number_value(found->second, path + "." + key);
}

eu_digital::HypothesisRecord parse_metacognition_hypothesis(const JsonValue& value, const std::string& path) {
    const auto& object = runtime_detail::object(value, path);
    eu_digital::HypothesisRecord hypothesis;
    hypothesis.hypothesis_id = required_string(object, "hypothesis_id", path);
    hypothesis.kind = required_string(object, "kind", path);
    hypothesis.statement = required_string(object, "statement", path);
    hypothesis.status = parse_hypothesis_status(required_string(object, "status", path));
    hypothesis.confidence = number_value(runtime_detail::required(object, "confidence", path), path + ".confidence");
    const auto& evidence = runtime_detail::object(runtime_detail::required(object, "evidence", path), path + ".evidence");
    hypothesis.supporting_refs = string_array(runtime_detail::required(evidence, "supporting_refs", path + ".evidence"), path + ".evidence.supporting_refs");
    hypothesis.opposing_refs = string_array(runtime_detail::required(evidence, "opposing_refs", path + ".evidence"), path + ".evidence.opposing_refs");
    hypothesis.alternatives = string_array(runtime_detail::required(object, "alternatives", path), path + ".alternatives");
    hypothesis.created_at = required_string(object, "created_at", path);
    hypothesis.updated_at = required_string(object, "updated_at", path);
    const auto& verification = runtime_detail::object(runtime_detail::required(object, "verification", path), path + ".verification");
    hypothesis.verification_question = optional_string(verification, "question", path + ".verification");
    hypothesis.expected_information_gain = optional_number(verification, "expected_information_gain", path + ".verification");
    const auto& provenance = runtime_detail::object(runtime_detail::required(object, "provenance", path), path + ".provenance");
    hypothesis.provenance_module = required_string(provenance, "module", path + ".provenance");
    hypothesis.model_version = optional_string(provenance, "model_version", path + ".provenance");
    hypothesis.schema_version = optional_string(object, "schema_version", path).value_or(eu_digital::METACOGNITION_CURIOSITY_SCHEMA_VERSION);
    hypothesis.validate();
    return hypothesis;
}

eu_digital::CuriosityConfig parse_metacognition_config(const JsonValue::Object& root) {
    eu_digital::CuriosityConfig config;
    const auto* object = optional_object(root, "config", "fixture");
    if (object == nullptr) return config;
    if (const auto found = object->find("calibration_enabled"); found != object->end()) config.calibration_enabled = runtime_detail::boolean(found->second, "fixture.config.calibration_enabled");
    if (const auto found = object->find("question_policy"); found != object->end()) config.question_policy = parse_question_policy(runtime_detail::string(found->second, "fixture.config.question_policy"));
    if (const auto found = object->find("interruptions_per_window"); found != object->end()) config.interruptions_per_window = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.interruptions_per_window"));
    if (const auto found = object->find("interruption_window_seconds"); found != object->end()) config.interruption_window_seconds = number_value(found->second, "fixture.config.interruption_window_seconds");
    if (const auto found = object->find("cooldown_seconds"); found != object->end()) config.cooldown_seconds = number_value(found->second, "fixture.config.cooldown_seconds");
    if (const auto found = object->find("correction_cooldown_seconds"); found != object->end()) config.correction_cooldown_seconds = number_value(found->second, "fixture.config.correction_cooldown_seconds");
    if (const auto found = object->find("min_information_gain"); found != object->end()) config.min_information_gain = number_value(found->second, "fixture.config.min_information_gain");
    if (const auto found = object->find("silence_confidence"); found != object->end()) config.silence_confidence = number_value(found->second, "fixture.config.silence_confidence");
    if (const auto found = object->find("redundancy_suppression_enabled"); found != object->end()) config.redundancy_suppression_enabled = runtime_detail::boolean(found->second, "fixture.config.redundancy_suppression_enabled");
    if (const auto found = object->find("budget_enabled"); found != object->end()) config.budget_enabled = runtime_detail::boolean(found->second, "fixture.config.budget_enabled");
    if (const auto found = object->find("cooldown_enabled"); found != object->end()) config.cooldown_enabled = runtime_detail::boolean(found->second, "fixture.config.cooldown_enabled");
    if (const auto found = object->find("calibration_bucket_count"); found != object->end()) config.calibration_bucket_count = static_cast<int>(runtime_detail::unsigned_number(found->second, "fixture.config.calibration_bucket_count"));
    config.validate();
    return config;
}

std::string metacognition_operation_time(const JsonValue::Object& operation, const std::string& path) {
    return optional_string(operation, "now", path).value_or("1970-01-01T00:00:00+00:00");
}

std::string metacognition_operation_id(const JsonValue::Object& operation, const std::string& direct_key,
                                       const std::string& reference_key, const std::string& last_value,
                                       const std::string& path) {
    if (const auto direct = optional_string(operation, direct_key, path)) return *direct;
    if (const auto reference = optional_string(operation, reference_key, path)) {
        if (*reference == "last") {
            if (last_value.empty()) throw std::runtime_error(path + " has no previous " + direct_key);
            return last_value;
        }
        if (*reference == "last_asked" && reference_key == "question_ref") {
            if (last_value.empty()) throw std::runtime_error(path + " has no previous asked question");
            return last_value;
        }
        throw std::runtime_error("unsupported " + reference_key + ": " + *reference);
    }
    throw std::runtime_error(path + " requires " + direct_key + " or " + reference_key);
}

int run_metacognition_curiosity() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        auto engine = eu_digital::MetacognitionCuriosityEngine(parse_metacognition_config(object));
        std::vector<eu_digital::MetacognitiveAssessment> assessments;
        std::vector<eu_digital::CuriosityQuestion> questions;
        std::vector<eu_digital::CuriosityResponse> responses;
        std::vector<std::string> snapshots;
        std::vector<std::string> metrics_reads;
        std::string last_assessment_id;
        std::string last_question_id;
        std::string last_asked_question_id;
        const auto& operations = runtime_detail::array(runtime_detail::required(object, "operations", "fixture"), "fixture.operations");
        for (std::size_t index = 0; index < operations.size(); ++index) {
            const auto path = "fixture.operations[" + std::to_string(index) + "]";
            const auto& operation = runtime_detail::object(operations[index], path);
            const auto type = required_string(operation, "type", path);
            if (type == "evaluate") {
                assessments.push_back(engine.evaluate(
                    parse_metacognition_hypothesis(runtime_detail::required(operation, "hypothesis", path), path + ".hypothesis"),
                    metacognition_operation_time(operation, path)));
                last_assessment_id = assessments.back().assessment_id;
            } else if (type == "propose_question") {
                questions.push_back(engine.propose_question(
                    metacognition_operation_id(operation, "assessment_id", "assessment_ref", last_assessment_id, path),
                    required_string(operation, "prompt", path),
                    number_value(runtime_detail::required(operation, "expected_resolution", path), path + ".expected_resolution"),
                    metacognition_operation_time(operation, path)));
                last_question_id = questions.back().question_id;
            } else if (type == "ask") {
                questions.push_back(engine.ask(metacognition_operation_id(operation, "question_id", "question_ref", last_question_id, path), metacognition_operation_time(operation, path)));
                last_question_id = questions.back().question_id;
                last_asked_question_id = last_question_id;
            } else if (type == "record_response") {
                const auto actor_id = optional_string(operation, "actor_id", path);
                responses.push_back(engine.record_response(
                    metacognition_operation_id(operation, "question_id", "question_ref", last_asked_question_id, path),
                    parse_response_outcome(required_string(operation, "outcome", path)),
                    runtime_detail::boolean(runtime_detail::required(operation, "correction", path), path + ".correction"),
                    string_array(runtime_detail::required(operation, "evidence_refs", path), path + ".evidence_refs"),
                    required_string(operation, "source", path), actor_id, metacognition_operation_time(operation, path)));
            } else if (type == "snapshot") {
                snapshots.push_back(engine.snapshot_json());
            } else if (type == "metrics") {
                metrics_reads.push_back(engine.metrics_json());
            } else {
                throw std::runtime_error("unsupported metacognition-curiosity operation: " + type);
            }
        }
        std::ostringstream output;
        output << "{\"assessments\":[";
        for (std::size_t index = 0; index < assessments.size(); ++index) {
            if (index) output << ',';
            output << assessments[index].to_json();
        }
        output << "],\"metrics_reads\":[";
        for (std::size_t index = 0; index < metrics_reads.size(); ++index) {
            if (index) output << ',';
            output << metrics_reads[index];
        }
        output << "],\"questions\":[";
        for (std::size_t index = 0; index < questions.size(); ++index) {
            if (index) output << ',';
            output << questions[index].to_json();
        }
        output << "],\"responses\":[";
        for (std::size_t index = 0; index < responses.size(); ++index) {
            if (index) output << ',';
            output << responses[index].to_json();
        }
        output << "],\"schema_version\":\"1.0\",\"snapshot\":" << engine.snapshot_json() << ",\"snapshots\":[";
        for (std::size_t index = 0; index < snapshots.size(); ++index) {
            if (index) output << ',';
            output << snapshots[index];
        }
        output << "]}\n";
        std::cout << output.str();
    }
    return std::cout.good() ? 0 : 1;
}

int run_episodic_memory() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        std::size_t max_episodes = 10000;
        if (const auto found = object.find("max_episodes"); found != object.end()) {
            max_episodes = static_cast<std::size_t>(runtime_detail::unsigned_number(found->second, "fixture.max_episodes"));
        }
        eu_digital::EpisodicMemoryStore memory(max_episodes);
        const auto& records = runtime_detail::array(runtime_detail::required(object, "records", "fixture"), "fixture.records");
        std::vector<std::string> store_results;
        for (std::size_t index = 0; index < records.size(); ++index) {
            const auto& record = runtime_detail::object(records[index], "fixture.records[" + std::to_string(index) + "]");
            auto episode = parse_memory_episode(runtime_detail::required(record, "episode", "fixture.records[" + std::to_string(index) + "]"), "fixture.records[" + std::to_string(index) + ".episode");
            std::optional<std::vector<double>> embedding;
            const auto found = record.find("embedding");
            if (found != record.end() && !runtime_detail::is_null(found->second)) embedding = number_array(found->second, "fixture.records[" + std::to_string(index) + "].embedding");
            store_results.push_back(memory.store(std::move(episode), std::move(embedding)));
        }
        eu_digital::MemoryQuery query;
        if (const auto* query_object = optional_object(object, "query", "fixture")) {
            query.session_id = optional_string(*query_object, "session_id", "fixture.query");
            if (const auto found = query_object->find("applications"); found != query_object->end()) query.applications = string_array(found->second, "fixture.query.applications");
            if (const auto found = query_object->find("documents"); found != query_object->end()) query.documents = string_array(found->second, "fixture.query.documents");
            if (const auto found = query_object->find("modalities"); found != query_object->end()) query.modalities = string_array(found->second, "fixture.query.modalities");
            if (const auto found = query_object->find("start_at"); found != query_object->end() && !runtime_detail::is_null(found->second)) query.start_epoch = parse_timestamp(runtime_detail::string(found->second, "fixture.query.start_at"));
            if (const auto found = query_object->find("end_at"); found != query_object->end() && !runtime_detail::is_null(found->second)) query.end_epoch = parse_timestamp(runtime_detail::string(found->second, "fixture.query.end_at"));
            if (const auto found = query_object->find("embedding"); found != query_object->end()) query.embedding = number_array(found->second, "fixture.query.embedding");
            if (const auto found = query_object->find("limit"); found != query_object->end()) query.limit = static_cast<std::size_t>(runtime_detail::unsigned_number(found->second, "fixture.query.limit"));
        }
        std::vector<std::string> consolidated;
        if (const auto found = object.find("consolidate"); found != object.end() && runtime_detail::boolean(found->second, "fixture.consolidate")) consolidated = memory.consolidate();
        double minimum_score = 0.0;
        if (const auto found = object.find("minimum_relation_score"); found != object.end()) minimum_score = number_value(found->second, "fixture.minimum_relation_score");
        std::cout << serialize_memory_result(store_results, consolidated, memory.size(), memory.retrieve(query), memory.similarity_relations(minimum_score));
    }
    return std::cout.good() ? 0 : 1;
}

std::string serialize_result(const eu_digital::EpisodeSegmentationResult& result) {
    std::ostringstream output;
    output << "{\"baseline_id\":" << json_string(result.baseline_id)
           << ",\"boundaries\":[";
    for (std::size_t index = 0; index < result.boundaries.size(); ++index) {
        if (index) output << ',';
        output << "{\"event_id\":" << json_string(result.boundaries[index].event_id)
               << ",\"reasons\":" << json_array(result.boundaries[index].reasons) << '}';
    }
    output << "],\"config_fingerprint\":" << json_string(result.config_fingerprint)
           << ",\"episodes\":[";
    for (std::size_t index = 0; index < result.episodes.size(); ++index) {
        if (index) output << ',';
        const auto& episode = result.episodes[index];
        output << "{\"boundary_reasons\":" << json_array(episode.boundary_reasons)
               << ",\"context_summary\":{\"applications\":" << json_array(episode.applications)
               << ",\"documents\":" << json_array(episode.documents)
               << ",\"modalities\":" << json_array(episode.modalities)
               << ",\"people\":[],\"topics\":[]},\"created_by\":\"episode_segmenter.threshold.v1\","
               << "\"embedding_ref\":null,\"end_at\":" << json_string(episode.end_at)
               << ",\"episode_id\":" << json_string(episode.episode_id)
               << ",\"event_ids\":" << json_array(episode.event_ids)
               << ",\"hypotheses\":[],\"quality\":{\"coherence\":1.0,\"confidence\":1.0},"
               << "\"schema_version\":\"1.0\",\"session_id\":" << json_string(episode.session_id)
               << ",\"start_at\":" << json_string(episode.start_at) << ",\"summary\":null}";
    }
    output << "],\"schema_version\":\"1.0\"}\n";
    return output.str();
}

int run_episode_segmentation() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        const auto root = runtime_detail::JsonParser(line).parse();
        const auto& object = runtime_detail::object(root, "fixture");
        eu_digital::EpisodeSegmentConfig config;
        if (const auto* config_object = optional_object(object, "config", "fixture")) {
            if (const auto found = config_object->find("max_gap_seconds"); found != config_object->end()) {
                config.max_gap_seconds = std::stod(std::get<runtime_detail::JsonNumber>(found->second.value).text);
            }
            if (const auto found = config_object->find("split_on_application_change"); found != config_object->end()) config.split_on_application_change = runtime_detail::boolean(found->second, "fixture.config.split_on_application_change");
            if (const auto found = config_object->find("split_on_document_change"); found != config_object->end()) config.split_on_document_change = runtime_detail::boolean(found->second, "fixture.config.split_on_document_change");
        }
        const auto& event_values = runtime_detail::array(runtime_detail::required(object, "events", "fixture"), "fixture.events");
        std::vector<eu_digital::EpisodeSegmentEvent> events;
        events.reserve(event_values.size());
        for (std::size_t index = 0; index < event_values.size(); ++index) events.push_back(parse_event(event_values[index], "fixture.events[" + std::to_string(index) + "]"));
        std::cout << serialize_result(eu_digital::EpisodeSegmenter::segment(events, config));
    }
    return std::cout.good() ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--episode-segmentation") return run_episode_segmentation();
        if (argc > 1 && std::string(argv[1]) == "--episodic-memory") return run_episodic_memory();
        if (argc > 1 && std::string(argv[1]) == "--pattern-learning") return run_pattern_learning();
        if (argc > 1 && std::string(argv[1]) == "--world-model") return run_world_model();
        if (argc > 1 && std::string(argv[1]) == "--global-workspace") return run_global_workspace();
        if (argc > 1 && std::string(argv[1]) == "--functional-self-model") return run_functional_self_model();
        if (argc > 1 && std::string(argv[1]) == "--metacognition-curiosity") return run_metacognition_curiosity();
        const std::string fixture_bytes{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
        std::cout << eu_digital::PromotionFixtureRunner::echo(fixture_bytes);
        return std::cout.good() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "promotion_fixture_runner_error: " << error.what() << '\n';
        return 2;
    }
}
