#pragma once

#include "core/contracts/port_result.hpp"
#include "core/contracts/prediction_assessment.hpp"
#include <string>
#include <vector>

namespace eu_digital {

class IPredictionPort {
public:
    virtual ~IPredictionPort() = default;

    virtual PredictionAssessment predict(
        const std::vector<std::string>& context,
        const std::string& predicted_at,
        const std::vector<std::string>& candidate_states = {}) = 0;

    virtual PredictionAssessment score(
        const PredictionAssessment& prediction,
        const std::string& observed_state,
        const std::string& observed_at) = 0;

    contracts::PortResult<PredictionAssessment> predict_result(
        const std::vector<std::string>& context,
        const std::string& predicted_at,
        const std::vector<std::string>& candidate_states = {}) {
        return contracts::capture_port_result<PredictionAssessment>(
            "prediction.predict",
            [&] { return predict(context, predicted_at, candidate_states); });
    }

    contracts::PortResult<PredictionAssessment> score_result(
        const PredictionAssessment& prediction,
        const std::string& observed_state,
        const std::string& observed_at) {
        return contracts::capture_port_result<PredictionAssessment>(
            "prediction.score",
            [&] { return score(prediction, observed_state, observed_at); });
    }
};

} // namespace eu_digital
