#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace eu_digital {

// Imutável
struct PredictionAssessment {
    std::string prediction_id;
    std::string stream_id;
    std::string model_id;
    std::vector<std::string> context;
    std::map<std::string, double> predicted_distribution;
    std::string predicted_at;
    int top_k;
    std::optional<std::string> observed_state;
    std::optional<double> log_loss;
    std::optional<bool> top_k_hit;
    double salience_contribution;
    double confidence;
    std::optional<std::string> drift_id;

    bool valid() const {
        return !prediction_id.empty() && !stream_id.empty() && !model_id.empty();
    }
};

} // namespace eu_digital
