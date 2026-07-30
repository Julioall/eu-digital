#pragma once

#include "core/capability_runtime.hpp"
#include "core/digest.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* WORLD_MODEL_SCHEMA_VERSION = "1.0";
inline constexpr const char* FREQUENCY_BASELINE_ID = "frequency_baseline_v0";
inline constexpr const char* MARKOV_BASELINE_ID = "markov_order1_v0";
inline constexpr const char* PREDICTOR_POLICY_ID = "incremental_markov_v1";
inline constexpr const char* WORLD_MODEL_CREATED_BY = "world_model.incremental_markov.v1";
inline constexpr const char* WORLD_MODEL_NAMESPACE = "c0e8d9f3-0e10-4e15-93d6-4aa1d73dba1e";

struct WorldModelConfig {
    int max_order{2};
    double smoothing{1.0};
    int drift_window{4};
    double drift_threshold{1.5};
    int top_k{3};

    void validate() const {
        if (max_order < 1) throw std::invalid_argument("max_order must be positive");
        if (!std::isfinite(smoothing) || smoothing <= 0.0) {
            throw std::invalid_argument("smoothing must be finite and positive");
        }
        if (drift_window < 1) throw std::invalid_argument("drift_window must be positive");
        if (!std::isfinite(drift_threshold) || drift_threshold < 0.0) {
            throw std::invalid_argument("drift_threshold must be finite and non-negative");
        }
        if (top_k < 1) throw std::invalid_argument("top_k must be positive");
    }
};

enum class WorldModelPolicy {
    frequency,
    markov,
    incremental,
};

struct PromotedPatternInput {
    std::string pattern_id;
    std::string status{"promoted"};
    double confidence{1.0};
};

inline WorldModelPolicy world_model_policy_from_id(const std::string& value) {
    if (value == FREQUENCY_BASELINE_ID) return WorldModelPolicy::frequency;
    if (value == MARKOV_BASELINE_ID) return WorldModelPolicy::markov;
    if (value == PREDICTOR_POLICY_ID) return WorldModelPolicy::incremental;
    throw std::invalid_argument("unsupported world model policy: " + value);
}

inline const char* world_model_policy_id(WorldModelPolicy policy) {
    switch (policy) {
    case WorldModelPolicy::frequency: return FREQUENCY_BASELINE_ID;
    case WorldModelPolicy::markov: return MARKOV_BASELINE_ID;
    case WorldModelPolicy::incremental: return PREDICTOR_POLICY_ID;
    }
    return PREDICTOR_POLICY_ID;
}

struct WorldPrediction {
    std::string prediction_id;
    std::string schema_version{WORLD_MODEL_SCHEMA_VERSION};
    std::string model_id;
    std::string stream_id;
    std::vector<std::string> context;
    std::map<std::string, double> predicted_distribution;
    std::string predicted_at;
    int top_k{1};
    std::optional<std::string> observed_state;
    std::optional<double> log_loss;
    std::optional<bool> top_k_hit;
    double salience_contribution{0.0};
    double confidence{1.0};
    std::optional<std::string> drift_id;
    std::string created_by{WORLD_MODEL_CREATED_BY};
};

struct WorldDriftSignal {
    std::string drift_id;
    std::string schema_version{WORLD_MODEL_SCHEMA_VERSION};
    std::string model_id;
    std::string stream_id;
    std::string detected_at;
    double rolling_log_loss{0.0};
    double threshold{0.0};
    double confidence_before{1.0};
    double confidence_after{1.0};
    bool relearning_started{true};
    std::string trigger_prediction_id;
    std::string reason;
};

class WorldModel {
public:
    WorldModel(WorldModelConfig config, std::string stream_id, WorldModelPolicy policy = WorldModelPolicy::incremental,
               std::vector<PromotedPatternInput> promoted_patterns = {})
        : config_(std::move(config)), stream_id_(std::move(stream_id)), policy_(policy),
          promoted_patterns_(normalize_promoted_patterns(promoted_patterns)) {
        config_.validate();
        if (stream_id_.empty()) throw std::invalid_argument("stream_id cannot be empty");
    }

    void observe(const std::string& state, const std::string& event_ref, double occurred_epoch) {
        require_state(state, "state");
        if (event_ref.empty()) throw std::invalid_argument("event_ref cannot be empty");
        if (!std::isfinite(occurred_epoch)) throw std::invalid_argument("observation timestamp must be finite");
        const auto previous_history = history_;
        states_.insert(state);
        global_counts_[state] += 1;
        if (policy_ != WorldModelPolicy::frequency) {
            const int order_limit = policy_ == WorldModelPolicy::markov ? 1 : config_.max_order;
            const int available = std::min(order_limit, static_cast<int>(previous_history.size()));
            for (int order = 1; order <= available; ++order) {
                const auto begin = previous_history.end() - order;
                transition_counts_[std::vector<std::string>(begin, previous_history.end())][state] += 1;
            }
        }
        history_.push_back(state);
        if (relearning_started_) ++relearning_observations_;
    }

    WorldPrediction predict(const std::vector<std::string>& context,
                            const std::string& predicted_at,
                            const std::vector<std::string>& candidate_states = {}) {
        if (predicted_at.empty()) throw std::invalid_argument("predicted_at cannot be empty");
        for (const auto& state : context) require_state(state, "context state");
        std::set<std::string> known(states_.begin(), states_.end());
        for (const auto& [pattern_id, unused] : promoted_patterns_) known.insert(pattern_id);
        for (const auto& state : candidate_states) {
            require_state(state, "candidate state");
            known.insert(state);
        }
        if (known.empty()) throw std::invalid_argument("at least one observed or candidate state is required");
        const std::vector<std::string> states(known.begin(), known.end());
        const auto distribution = make_distribution(context, states);
        ++sequence_;
        WorldPrediction prediction;
        prediction.prediction_id = digest::uuid5(
            WORLD_MODEL_NAMESPACE,
            stream_id_ + ":" + std::string(world_model_policy_id(policy_)) + ":" + std::to_string(sequence_));
        prediction.model_id = world_model_policy_id(policy_);
        prediction.stream_id = stream_id_;
        prediction.context = context;
        prediction.predicted_distribution = distribution;
        prediction.predicted_at = predicted_at;
        prediction.top_k = std::min(config_.top_k, static_cast<int>(states.size()));
        prediction.confidence = confidence_;
        predictions_[prediction.prediction_id] = prediction;
        return prediction;
    }

    WorldPrediction score(const WorldPrediction& prediction,
                          const std::string& observed_state,
                          const std::string& observed_at) {
        require_state(observed_state, "observed_state");
        if (observed_at.empty()) throw std::invalid_argument("observed_at cannot be empty");
        const auto found = predictions_.find(prediction.prediction_id);
        if (found == predictions_.end() || !same_prediction(found->second, prediction)) {
            throw std::invalid_argument("prediction does not belong to this model");
        }
        if (found->second.log_loss.has_value()) throw std::invalid_argument("prediction has already been scored");
        const auto probability = prediction.predicted_distribution.contains(observed_state)
            ? prediction.predicted_distribution.at(observed_state) : 0.0;
        const double log_loss = -std::log(std::max(probability, 1e-12));
        const auto ranked = ranked_states(prediction.predicted_distribution);
        bool top_k_hit = false;
        const auto limit = std::min(prediction.top_k, static_cast<int>(ranked.size()));
        for (int index = 0; index < limit; ++index) {
            if (ranked[static_cast<std::size_t>(index)].first == observed_state) {
                top_k_hit = true;
                break;
            }
        }
        WorldPrediction scored = prediction;
        scored.observed_state = observed_state;
        scored.log_loss = log_loss;
        scored.top_k_hit = top_k_hit;
        scored.salience_contribution = prediction_error_to_salience(log_loss);
        errors_.push_back(log_loss);
        rolling_errors_.push_back(log_loss);
        while (rolling_errors_.size() > static_cast<std::size_t>(config_.drift_window)) rolling_errors_.pop_front();
        const auto drift = detect_drift(scored, observed_at);
        if (drift) {
            scored.confidence = drift->confidence_after;
            scored.drift_id = drift->drift_id;
            scored.salience_contribution = std::max(scored.salience_contribution, 0.9);
            drifts_.push_back(*drift);
        } else {
            scored.confidence = confidence_;
        }
        predictions_[prediction.prediction_id] = scored;
        return scored;
    }

    const std::vector<WorldDriftSignal>& drifts() const { return drifts_; }
    const std::string& stream_id() const { return stream_id_; }
    WorldModelPolicy policy() const { return policy_; }
    const WorldModelConfig& config() const { return config_; }
    std::size_t prediction_count() const { return predictions_.size(); }
    std::size_t scored_count() const { return errors_.size(); }
    std::optional<double> mean_log_loss() const {
        if (errors_.empty()) return std::nullopt;
        double total = 0.0;
        for (const auto value : errors_) total += value;
        return total / static_cast<double>(errors_.size());
    }
    std::optional<double> top_k_accuracy() const {
        if (errors_.empty()) return std::nullopt;
        std::size_t hits = 0;
        for (const auto& [unused, prediction] : predictions_) {
            if (prediction.top_k_hit.value_or(false)) ++hits;
        }
        return static_cast<double>(hits) / static_cast<double>(errors_.size());
    }
    double confidence() const { return confidence_; }
    bool relearning_started() const { return relearning_started_; }
    int relearning_observations() const { return relearning_observations_; }
    std::size_t promoted_pattern_count() const { return promoted_patterns_.size(); }

private:
    using Counts = std::map<std::string, int>;
    using TransitionCounts = std::map<std::vector<std::string>, Counts>;

    static void require_state(const std::string& value, const std::string& field) {
        if (value.empty()) throw std::invalid_argument(field + " must be a non-empty string");
    }

    static std::map<std::string, PromotedPatternInput> normalize_promoted_patterns(
        const std::vector<PromotedPatternInput>& patterns) {
        std::map<std::string, PromotedPatternInput> normalized;
        for (const auto& pattern : patterns) {
            if (pattern.pattern_id.empty()) throw std::invalid_argument("pattern_id cannot be empty");
            if (pattern.status != "promoted") throw std::invalid_argument("world model accepts only promoted patterns");
            if (!std::isfinite(pattern.confidence) || pattern.confidence < 0.0 || pattern.confidence > 1.0) {
                throw std::invalid_argument("pattern confidence must be between zero and one");
            }
            if (!normalized.emplace(pattern.pattern_id, pattern).second) {
                throw std::invalid_argument("duplicate promoted pattern: " + pattern.pattern_id);
            }
        }
        return normalized;
    }

    static bool same_prediction(const WorldPrediction& first, const WorldPrediction& second) {
        return first.prediction_id == second.prediction_id && first.model_id == second.model_id &&
            first.stream_id == second.stream_id && first.context == second.context &&
            first.predicted_distribution == second.predicted_distribution && first.predicted_at == second.predicted_at &&
            first.top_k == second.top_k && first.observed_state == second.observed_state &&
            first.log_loss == second.log_loss && first.top_k_hit == second.top_k_hit &&
            first.confidence == second.confidence;
    }

    std::map<std::string, double> make_distribution(const std::vector<std::string>& context,
                                                    const std::vector<std::string>& states) const {
        const Counts* counts = nullptr;
        if (policy_ == WorldModelPolicy::frequency) {
            counts = &global_counts_;
        } else {
            const int order_limit = policy_ == WorldModelPolicy::markov ? 1 : config_.max_order;
            const int bounded_size = std::min(order_limit, static_cast<int>(context.size()));
            for (int order = bounded_size; order >= 1; --order) {
                const auto begin = context.end() - order;
                const auto found = transition_counts_.find(std::vector<std::string>(begin, context.end()));
                if (found != transition_counts_.end()) {
                    counts = &found->second;
                    break;
                }
            }
            if (counts == nullptr) counts = &global_counts_;
        }
        const double denominator = [&] {
            double result = config_.smoothing * static_cast<double>(states.size());
            for (const auto& state : states) {
                const auto found = counts->find(state);
                if (found != counts->end()) result += static_cast<double>(found->second);
            }
            return result;
        }();
        std::map<std::string, double> distribution;
        for (const auto& state : states) {
            const auto found = counts->find(state);
            const int count = found == counts->end() ? 0 : found->second;
            distribution[state] = (static_cast<double>(count) + config_.smoothing) / denominator;
        }
        return distribution;
    }

    static std::vector<std::pair<std::string, double>> ranked_states(const std::map<std::string, double>& distribution) {
        std::vector<std::pair<std::string, double>> ranked(distribution.begin(), distribution.end());
        std::sort(ranked.begin(), ranked.end(), [](const auto& left, const auto& right) {
            if (left.second != right.second) return left.second > right.second;
            return left.first < right.first;
        });
        return ranked;
    }

    static double prediction_error_to_salience(double log_loss) {
        if (!std::isfinite(log_loss) || log_loss < 0.0) {
            throw std::invalid_argument("log_loss must be finite and non-negative");
        }
        return std::min(1.0, 1.0 - std::exp(-log_loss));
    }

    std::optional<WorldDriftSignal> detect_drift(const WorldPrediction& prediction, const std::string& observed_at) {
        if (rolling_errors_.size() < static_cast<std::size_t>(config_.drift_window)) return std::nullopt;
        double rolling_loss = 0.0;
        for (const auto value : rolling_errors_) rolling_loss += value;
        rolling_loss /= static_cast<double>(rolling_errors_.size());
        if (rolling_loss <= config_.drift_threshold) {
            drift_latched_ = false;
            return std::nullopt;
        }
        if (drift_latched_) return std::nullopt;
        const double before = confidence_;
        confidence_ = std::max(0.1, before * 0.5);
        transition_counts_.clear();
        relearning_started_ = true;
        const auto drift_id = digest::uuid5(
            WORLD_MODEL_NAMESPACE,
            "drift:" + stream_id_ + ":" + std::to_string(drifts_.size() + 1));
        drift_latched_ = true;
        return WorldDriftSignal{
            drift_id,
            WORLD_MODEL_SCHEMA_VERSION,
            world_model_policy_id(policy_),
            stream_id_,
            observed_at,
            rolling_loss,
            config_.drift_threshold,
            before,
            confidence_,
            true,
            prediction.prediction_id,
            "rolling_prediction_error_exceeded_threshold",
        };
    }

    WorldModelConfig config_;
    std::string stream_id_;
    WorldModelPolicy policy_;
    std::map<std::string, PromotedPatternInput> promoted_patterns_;
    Counts global_counts_;
    TransitionCounts transition_counts_;
    std::set<std::string> states_;
    std::vector<std::string> history_;
    std::map<std::string, WorldPrediction> predictions_;
    std::vector<double> errors_;
    std::deque<double> rolling_errors_;
    std::vector<WorldDriftSignal> drifts_;
    double confidence_{1.0};
    bool drift_latched_{false};
    bool relearning_started_{false};
    int relearning_observations_{0};
    std::size_t sequence_{0};
};

inline double prediction_error_to_salience(double log_loss) {
    if (!std::isfinite(log_loss) || log_loss < 0.0) {
        throw std::invalid_argument("log_loss must be finite and non-negative");
    }
    return std::min(1.0, 1.0 - std::exp(-log_loss));
}

class WorldModelPlugin final : public CapabilityPlugin {
public:
    WorldModelPlugin() {
        descriptor_.capability_id = "cognition.world_model";
        descriptor_.implementation_id = "native.world_model";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "cognitive_service";
        descriptor_.provides.push_back({"predict.transitions", "urn:eu-digital:world-model:1"});
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
