#pragma once

#include <string>
#include <vector>

namespace eu_digital {

struct SelfConstraintSnapshot {
    std::string model_id;
    std::vector<std::string> active_constraints;
    double alignment_score;

    bool valid() const {
        return !model_id.empty();
    }
};

} // namespace eu_digital
