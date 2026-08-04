#pragma once

#include "core/contracts/pattern_learning.hpp"

#include <vector>

namespace eu_digital {

class IPatternLearningPort {
public:
    virtual ~IPatternLearningPort() = default;

    virtual contracts::PatternLearningResult observe(
        const contracts::PatternLearningObservation& observation) = 0;

    virtual contracts::PatternLearningResult feedback(
        const contracts::PatternLearningFeedback& feedback) = 0;

    virtual std::vector<contracts::ObservedPattern> snapshot() const = 0;

    contracts::PortResult<std::vector<contracts::ObservedPattern>> snapshot_result() const {
        return contracts::capture_port_result<std::vector<contracts::ObservedPattern>>(
            "pattern_learning.snapshot", [&] { return snapshot(); });
    }
};

}  // namespace eu_digital
