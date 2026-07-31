#pragma once

#include "core/ports/iprediction_port.hpp"
#include "core/world_model.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class WorldModelAdapter final : public IPredictionPort {
public:
    explicit WorldModelAdapter(std::shared_ptr<WorldModel> world_model)
        : world_model_(std::move(world_model)) {
        if (!world_model_) {
            throw std::invalid_argument("world_model cannot be null");
        }
    }

    PredictionAssessment predict(
        const std::vector<std::string>& context,
        const std::string& predicted_at,
        const std::vector<std::string>& candidate_states = {}) override {
        
        std::lock_guard lock(mutex_);
        try {
            auto wp = world_model_->predict(context, predicted_at, candidate_states);
            
            PredictionAssessment assessment;
            assessment.prediction_id = wp.prediction_id;
            assessment.stream_id = wp.stream_id;
            assessment.model_id = wp.model_id;
            assessment.context = wp.context;
            assessment.predicted_distribution = wp.predicted_distribution;
            assessment.predicted_at = wp.predicted_at;
            assessment.top_k = wp.top_k;
            assessment.observed_state = wp.observed_state;
            assessment.log_loss = wp.log_loss;
            assessment.top_k_hit = wp.top_k_hit;
            assessment.salience_contribution = wp.salience_contribution;
            assessment.confidence = wp.confidence;
            assessment.drift_id = wp.drift_id;
            
            return assessment;
        } catch (const std::exception& e) {
            PredictionAssessment failed;
            // Retorna vazio ou invalido
            return failed;
        }
    }

    PredictionAssessment score(
        const PredictionAssessment& prediction,
        const std::string& observed_state,
        const std::string& observed_at) override {
        
        std::lock_guard lock(mutex_);
        try {
            // Recriar o WorldPrediction pro world_model (ou seria melhor guardar internamente?)
            // A API pede um WorldPrediction, mas prediction_assessment tem os dados.
            WorldPrediction wp;
            wp.prediction_id = prediction.prediction_id;
            wp.model_id = prediction.model_id;
            wp.stream_id = prediction.stream_id;
            wp.context = prediction.context;
            wp.predicted_distribution = prediction.predicted_distribution;
            wp.predicted_at = prediction.predicted_at;
            wp.top_k = prediction.top_k;
            wp.observed_state = prediction.observed_state;
            wp.log_loss = prediction.log_loss;
            wp.top_k_hit = prediction.top_k_hit;
            wp.salience_contribution = prediction.salience_contribution;
            wp.confidence = prediction.confidence;
            wp.drift_id = prediction.drift_id;

            auto scored_wp = world_model_->score(wp, observed_state, observed_at);

            PredictionAssessment assessment;
            assessment.prediction_id = scored_wp.prediction_id;
            assessment.stream_id = scored_wp.stream_id;
            assessment.model_id = scored_wp.model_id;
            assessment.context = scored_wp.context;
            assessment.predicted_distribution = scored_wp.predicted_distribution;
            assessment.predicted_at = scored_wp.predicted_at;
            assessment.top_k = scored_wp.top_k;
            assessment.observed_state = scored_wp.observed_state;
            assessment.log_loss = scored_wp.log_loss;
            assessment.top_k_hit = scored_wp.top_k_hit;
            assessment.salience_contribution = scored_wp.salience_contribution;
            assessment.confidence = scored_wp.confidence;
            assessment.drift_id = scored_wp.drift_id;
            
            return assessment;
        } catch (const std::exception& e) {
            PredictionAssessment failed;
            return failed;
        }
    }

private:
    std::shared_ptr<WorldModel> world_model_;
    std::mutex mutex_;
};

} // namespace eu_digital
