#pragma once

#include "core/capability_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

struct MemoryEpisode {
    std::string episode_id;
    std::string schema_version{"1.0"};
    std::string session_id;
    std::string start_at;
    std::string end_at;
    double start_epoch{0.0};
    double end_epoch{0.0};
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

    void validate() const {
        if (episode_id.empty()) throw std::invalid_argument("episode_id is required");
        if (schema_version != "1.0") throw std::invalid_argument("unsupported episode schema_version");
        if (session_id.empty()) throw std::invalid_argument("session_id is required");
        if (start_at.empty() || end_at.empty()) throw std::invalid_argument("episode timestamps are required");
        if (created_by.empty()) throw std::invalid_argument("created_by is required");
        if (std::any_of(boundary_reasons.begin(), boundary_reasons.end(), [](const auto& reason) { return reason.empty(); })) {
            throw std::invalid_argument("episode boundary reasons must not be empty");
        }
        if (!std::isfinite(start_epoch) || !std::isfinite(end_epoch) || end_epoch < start_epoch) {
            throw std::invalid_argument("episode time range is invalid");
        }
        if (!std::isfinite(coherence) || coherence < 0.0 || coherence > 1.0 ||
            !std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) {
            throw std::invalid_argument("episode quality must be between zero and one");
        }
    }
};

struct MemoryQuery {
    std::optional<std::string> session_id;
    std::vector<std::string> applications;
    std::vector<std::string> documents;
    std::vector<std::string> modalities;
    std::optional<double> start_epoch;
    std::optional<double> end_epoch;
    std::optional<std::vector<double>> embedding;
    std::size_t limit{10};

    void validate() const {
        if (limit == 0) throw std::invalid_argument("memory query limit must be positive");
        if (embedding && embedding->empty()) throw std::invalid_argument("embedding must not be empty");
        if (embedding) {
            for (const auto value : *embedding) {
                if (!std::isfinite(value)) throw std::invalid_argument("embedding must contain finite values");
            }
        }
    }
};

struct MemoryRetrievalResult {
    MemoryEpisode episode;
    double score{0.0};
    std::vector<std::string> reason_codes;
};

struct MemoryRelation {
    std::string episode_a;
    std::string episode_b;
    double score{0.0};
    std::vector<std::string> reason_codes;
    std::vector<std::string> event_ids;
};

class EpisodicMemoryStore {
public:
    explicit EpisodicMemoryStore(std::size_t max_episodes = 10000)
        : max_episodes_(max_episodes) {
        if (max_episodes_ == 0) throw std::invalid_argument("max_episodes must be positive");
    }

    std::string store(MemoryEpisode episode, std::optional<std::vector<double>> embedding = std::nullopt) {
        episode.validate();
        if (embedding && !valid_vector(*embedding)) throw std::invalid_argument("embedding must contain finite values");
        const auto episode_id = episode.episode_id;
        if (episodes_.contains(episode_id)) return "duplicate";
        episodes_.emplace(episode_id, std::move(episode));
        if (embedding && valid_vector(*embedding)) embeddings_[episode_id] = std::move(*embedding);
        return "accepted";
    }

    std::vector<MemoryRetrievalResult> retrieve(const MemoryQuery& query) const {
        query.validate();
        std::vector<MemoryRetrievalResult> results;
        for (const auto& [episode_id, episode] : episodes_) {
            auto result = match(episode, query);
            if (result) results.push_back(std::move(*result));
        }
        std::sort(results.begin(), results.end(), [](const auto& left, const auto& right) {
            if (left.score != right.score) return left.score > right.score;
            if (left.episode.start_at != right.episode.start_at) return left.episode.start_at < right.episode.start_at;
            return left.episode.episode_id < right.episode.episode_id;
        });
        if (results.size() > query.limit) results.resize(query.limit);
        return results;
    }

    std::vector<MemoryRelation> similarity_relations(double minimum_score = 0.0) const {
        if (minimum_score < 0.0 || minimum_score > 1.0) throw std::invalid_argument("minimum_score must be between zero and one");
        std::vector<MemoryRelation> relations;
        for (auto left = episodes_.begin(); left != episodes_.end(); ++left) {
            for (auto right = std::next(left); right != episodes_.end(); ++right) {
                MemoryRelation relation;
                relation.episode_a = left->first;
                relation.episode_b = right->first;
                for (const auto& field : context_fields(left->second, right->second)) {
                    relation.reason_codes.push_back(field.first);
                    relation.score += field.second;
                }
                const auto left_embedding = embeddings_.find(left->first);
                const auto right_embedding = embeddings_.find(right->first);
                if (left_embedding != embeddings_.end() && right_embedding != embeddings_.end() &&
                    left_embedding->second.size() == right_embedding->second.size()) {
                    relation.score = std::max(relation.score, std::max(0.0, cosine(left_embedding->second, right_embedding->second)));
                    if (relation.score > 0.0 && std::find(relation.reason_codes.begin(), relation.reason_codes.end(), "embedding.cosine") == relation.reason_codes.end()) {
                        relation.reason_codes.emplace_back("embedding.cosine");
                    }
                }
                if (relation.score >= minimum_score && !relation.reason_codes.empty()) {
                    relation.event_ids = left->second.event_ids;
                    relation.event_ids.insert(relation.event_ids.end(), right->second.event_ids.begin(), right->second.event_ids.end());
                    relations.push_back(std::move(relation));
                }
            }
        }
        std::sort(relations.begin(), relations.end(), [](const auto& left, const auto& right) {
            if (left.score != right.score) return left.score > right.score;
            if (left.episode_a != right.episode_a) return left.episode_a < right.episode_a;
            return left.episode_b < right.episode_b;
        });
        return relations;
    }

    std::vector<std::string> consolidate() {
        if (episodes_.size() <= max_episodes_) return {};
        std::vector<std::string> ordered;
        for (const auto& [episode_id, episode] : episodes_) ordered.push_back(episode_id);
        std::sort(ordered.begin(), ordered.end(), [&](const auto& left, const auto& right) {
            const auto& a = episodes_.at(left);
            const auto& b = episodes_.at(right);
            if (a.end_epoch != b.end_epoch) return a.end_epoch > b.end_epoch;
            return left > right;
        });
        std::vector<std::string> removed;
        for (std::size_t index = max_episodes_; index < ordered.size(); ++index) {
            removed.push_back(ordered[index]);
            episodes_.erase(ordered[index]);
            embeddings_.erase(ordered[index]);
        }
        std::sort(removed.begin(), removed.end());
        return removed;
    }

    std::size_t size() const { return episodes_.size(); }
    bool contains(const std::string& episode_id) const { return episodes_.contains(episode_id); }

private:
    std::optional<MemoryRetrievalResult> match(const MemoryEpisode& episode, const MemoryQuery& query) const {
        std::vector<std::string> reasons;
        double score = 0.0;
        if (query.session_id) {
            if (episode.session_id != *query.session_id) return std::nullopt;
            reasons.emplace_back("session.match");
            score += 0.1;
        }
        if (!query.applications.empty() && !matches_any(query.applications, episode.applications)) return std::nullopt;
        if (!query.applications.empty()) {
            reasons.emplace_back("context.application");
            score += 0.35;
        }
        if (!query.documents.empty() && !matches_any(query.documents, episode.documents)) return std::nullopt;
        if (!query.documents.empty()) {
            reasons.emplace_back("context.document");
            score += 0.35;
        }
        if (!query.modalities.empty() && !matches_any(query.modalities, episode.modalities)) return std::nullopt;
        if (!query.modalities.empty()) {
            reasons.emplace_back("context.modality");
            score += 0.15;
        }
        if (query.start_epoch || query.end_epoch) {
            if (query.end_epoch && episode.start_epoch > *query.end_epoch) return std::nullopt;
            if (query.start_epoch && episode.end_epoch < *query.start_epoch) return std::nullopt;
            reasons.emplace_back("temporal.overlap");
            score += 0.05;
        }
        if (query.embedding) {
            const auto found = embeddings_.find(episode.episode_id);
            if (found == embeddings_.end() || found->second.size() != query.embedding->size()) return std::nullopt;
            const double similarity = cosine(found->second, *query.embedding);
            if (similarity <= 0.0) return std::nullopt;
            reasons.emplace_back("embedding.cosine");
            score += similarity;
        }
        if (reasons.empty()) reasons.emplace_back("chronological.fallback");
        return MemoryRetrievalResult{episode, score, std::move(reasons)};
    }

    static bool matches_any(const std::vector<std::string>& requested, const std::vector<std::string>& observed) {
        if (requested.empty()) return true;
        for (const auto& value : requested) if (std::find(observed.begin(), observed.end(), value) != observed.end()) return true;
        return false;
    }

    static std::vector<std::pair<std::string, double>> context_fields(const MemoryEpisode& left, const MemoryEpisode& right) {
        std::vector<std::pair<std::string, double>> fields;
        if (overlap(left.applications, right.applications)) fields.emplace_back("context.application", 1.0 / 3.0);
        if (overlap(left.documents, right.documents)) fields.emplace_back("context.document", 1.0 / 3.0);
        if (overlap(left.modalities, right.modalities)) fields.emplace_back("context.modality", 1.0 / 3.0);
        return fields;
    }

    static bool overlap(const std::vector<std::string>& left, const std::vector<std::string>& right) {
        return std::any_of(left.begin(), left.end(), [&](const auto& value) { return std::find(right.begin(), right.end(), value) != right.end(); });
    }

    static bool valid_vector(const std::vector<double>& vector) {
        return !vector.empty() && std::all_of(vector.begin(), vector.end(), [](double value) { return std::isfinite(value); });
    }

    static double cosine(const std::vector<double>& left, const std::vector<double>& right) {
        double numerator = 0.0, left_norm = 0.0, right_norm = 0.0;
        for (std::size_t index = 0; index < left.size(); ++index) {
            numerator += left[index] * right[index];
            left_norm += left[index] * left[index];
            right_norm += right[index] * right[index];
        }
        if (left_norm == 0.0 || right_norm == 0.0) return 0.0;
        return numerator / (std::sqrt(left_norm) * std::sqrt(right_norm));
    }

    std::size_t max_episodes_;
    std::map<std::string, MemoryEpisode> episodes_;
    std::map<std::string, std::vector<double>> embeddings_;
};

class EpisodicMemoryPlugin final : public CapabilityPlugin {
public:
    EpisodicMemoryPlugin() {
        descriptor_.capability_id = "cognition.episodic_memory";
        descriptor_.implementation_id = "native.episodic_memory";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"memory.retrieve", "urn:eu-digital:episodic-memory:1"});
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
