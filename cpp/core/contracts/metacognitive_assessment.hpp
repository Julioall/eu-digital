#pragma once

#include <string>

namespace eu_digital {

struct MetacognitiveAssessment {
    std::string assessment_id;
    double curiosity_score;
    bool requires_exploration;
    std::string focus_area;

    bool valid() const {
        return !assessment_id.empty();
    }
};

} // namespace eu_digital
