#pragma once

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
};

} // namespace eu_digital
