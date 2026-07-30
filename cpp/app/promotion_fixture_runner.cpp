#include "core/episode_segmenter.hpp"
#include "core/episodic_memory.hpp"
#include "core/pattern_learner.hpp"
#include "core/promotion_registry.hpp"
#include "core/runtime_host.hpp"

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
        const std::string fixture_bytes{std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>()};
        std::cout << eu_digital::PromotionFixtureRunner::echo(fixture_bytes);
        return std::cout.good() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "promotion_fixture_runner_error: " << error.what() << '\n';
        return 2;
    }
}
