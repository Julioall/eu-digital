#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct PatternConfig {
    int min_support{3};
    double distance_threshold{0.25};
    double promotion_confidence{0.5};

    void validate() const {
        if (min_support < 1) throw std::invalid_argument("min_support must be positive");
        if (!std::isfinite(distance_threshold) || distance_threshold < 0.0) throw std::invalid_argument("distance_threshold must be finite and non-negative");
        if (!std::isfinite(promotion_confidence) || promotion_confidence < 0.0 || promotion_confidence > 1.0) throw std::invalid_argument("promotion_confidence must be between zero and one");
    }
};

struct PatternRecord {
    std::string pattern_id;
    std::string schema_version{"1.0"};
    int version{1};
    std::string status{"candidate"};
    std::map<std::string, double> centroid;
    int support{1};
    double stability{0.0};
    double recency{1.0};
    double confidence{0.5};
    std::vector<std::string> observation_refs;
    int positive_feedback{0};
    int negative_feedback{0};
    std::vector<std::string> feedback_references;
    std::optional<std::string> parent_pattern_id;
    std::optional<std::string> drift_reason;
    std::string created_by{"pattern_learner.distance_threshold.v1"};
};

struct PatternObservation {
    std::map<std::string, double> features;
    std::string observation_ref;
    double occurred_epoch{0.0};
};

class PatternLearner {
public:
    PatternLearner(PatternConfig config, std::string stream_id)
        : config_(std::move(config)), stream_id_(std::move(stream_id)) {
        config_.validate();
        if (stream_id_.empty()) throw std::invalid_argument("stream_id cannot be empty");
    }

    PatternRecord observe(const PatternObservation& observation) {
        validate_observation(observation);
        ++clock_;
        for (auto& [unused, record] : patterns_) {
            if (record.status != "superseded") record.recency = std::max(0.0, record.recency * 0.99);
        }
        const auto vector = normalized(observation.features);
        PatternRecord* nearest = find_nearest(vector, false);
        if (nearest != nullptr) {
            update(*nearest, vector, observation.observation_ref);
            return *nearest;
        }
        PatternRecord* parent = find_nearest(vector, true);
        int version = 1;
        std::optional<std::string> parent_id;
        std::optional<std::string> drift_reason;
        if (parent != nullptr) {
            parent->status = "superseded";
            version = parent->version + 1;
            parent_id = parent->pattern_id;
            drift_reason = "concept_drift";
        }
        PatternRecord record;
        record.pattern_id = digest::uuid5(
            "e756dcc0-b35a-43f6-a7d1-30e89f4f1b55",
            stream_id_ + ":" + (parent_id ? *parent_id : "root") + ":" + std::to_string(version));
        record.version = version;
        record.centroid = vector;
        record.support = 1;
        record.stability = std::min(1.0, 1.0 / static_cast<double>(config_.min_support));
        record.observation_refs.push_back(observation.observation_ref);
        record.parent_pattern_id = parent_id;
        record.drift_reason = drift_reason;
        refresh_status(record);
        patterns_[record.pattern_id] = record;
        return record;
    }

    PatternRecord feedback(const std::string& pattern_id, bool positive, const std::string& reference) {
        if (reference.empty()) throw std::invalid_argument("feedback reference cannot be empty");
        auto found = patterns_.find(pattern_id);
        if (found == patterns_.end()) throw std::invalid_argument("unknown pattern: " + pattern_id);
        auto& record = found->second;
        if (positive) ++record.positive_feedback;
        else ++record.negative_feedback;
        record.feedback_references.push_back(reference);
        const int total = record.positive_feedback + record.negative_feedback;
        record.confidence = static_cast<double>(record.positive_feedback + 1) / static_cast<double>(total + 2);
        refresh_status(record);
        return record;
    }

    std::vector<PatternRecord> records() const {
        std::vector<PatternRecord> result;
        for (const auto& [unused, record] : patterns_) result.push_back(record);
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            if (left.pattern_id != right.pattern_id) return left.pattern_id < right.pattern_id;
            return left.version < right.version;
        });
        return result;
    }

    std::map<std::string, double> metrics() const {
        std::map<std::string, double> result;
        result["cluster_count"] = static_cast<double>(patterns_.size());
        double promoted = 0.0;
        double false_discovery_sum = 0.0;
        for (const auto& [unused, record] : patterns_) {
            if (record.status == "promoted") promoted += 1.0;
            const int total = record.positive_feedback + record.negative_feedback;
            false_discovery_sum += total == 0 ? 0.0 : static_cast<double>(record.negative_feedback) / total;
        }
        result["promoted_count"] = promoted;
        result["mean_false_discovery_rate"] = patterns_.empty() ? 0.0 : false_discovery_sum / patterns_.size();
        return result;
    }

    const PatternConfig& config() const { return config_; }
    const std::string& stream_id() const { return stream_id_; }

private:
    static void validate_observation(const PatternObservation& observation) {
        if (observation.observation_ref.empty()) throw std::invalid_argument("observation_ref cannot be empty");
        if (!std::isfinite(observation.occurred_epoch)) throw std::invalid_argument("observation timestamp must be finite");
        if (observation.features.empty()) throw std::invalid_argument("at least one numeric feature is required");
        for (const auto& [key, value] : observation.features) {
            if (key.empty() || !std::isfinite(value)) throw std::invalid_argument("features must be finite and named");
        }
    }

    static std::map<std::string, double> normalized(const std::map<std::string, double>& features) {
        return features;
    }

    static std::optional<double> distance(const std::map<std::string, double>& first, const std::map<std::string, double>& second) {
        double sum = 0.0;
        int shared = 0;
        for (const auto& [key, value] : first) {
            const auto found = second.find(key);
            if (found == second.end()) continue;
            const double difference = value - found->second;
            sum += difference * difference;
            ++shared;
        }
        if (shared == 0) return std::nullopt;
        return std::sqrt(sum / static_cast<double>(shared));
    }

    PatternRecord* find_nearest(const std::map<std::string, double>& vector, bool include_superseded) {
        PatternRecord* selected = nullptr;
        double selected_distance = 0.0;
        for (auto& [pattern_id, record] : patterns_) {
            if (!include_superseded && record.status == "superseded") continue;
            const auto candidate_distance = distance(vector, record.centroid);
            if (!candidate_distance) continue;
            if (selected == nullptr || *candidate_distance < selected_distance ||
                (*candidate_distance == selected_distance && pattern_id < selected->pattern_id)) {
                selected = &record;
                selected_distance = *candidate_distance;
            }
        }
        if (!include_superseded && selected != nullptr && selected_distance > config_.distance_threshold) return nullptr;
        return selected;
    }

    void update(PatternRecord& record, const std::map<std::string, double>& vector, const std::string& observation_ref) {
        const int old_support = record.support;
        for (const auto& [key, value] : vector) {
            const auto found = record.centroid.find(key);
            const double previous = found == record.centroid.end() ? value : found->second;
            record.centroid[key] = (previous * old_support + value) / static_cast<double>(old_support + 1);
        }
        ++record.support;
        record.stability = std::min(1.0, static_cast<double>(record.support) / static_cast<double>(config_.min_support));
        record.recency = 1.0;
        record.observation_refs.push_back(observation_ref);
        refresh_status(record);
    }

    void refresh_status(PatternRecord& record) const {
        if (record.status == "superseded") return;
        record.status = record.support >= config_.min_support && record.confidence >= config_.promotion_confidence ? "promoted" : "candidate";
    }

    PatternConfig config_;
    std::string stream_id_;
    std::map<std::string, PatternRecord> patterns_;
    int clock_{0};
};

class PatternLearningPlugin final : public CapabilityPlugin {
public:
    PatternLearningPlugin() {
        descriptor_.capability_id = "cognition.pattern_learning";
        descriptor_.implementation_id = "native.pattern_learner";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"learn.patterns", "urn:eu-digital:pattern:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
    }
    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    CapabilityDescriptor descriptor_;
};

}  // namespace eu_digital
