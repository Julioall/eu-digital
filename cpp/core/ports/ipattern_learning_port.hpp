#pragma once

#include "core/contracts/pattern_learning.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"

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

    virtual contracts::PatternLearningResult observe_context(
        const contracts::PatternLearningObservation& observation,
        const contracts::PortInvocationContextV1& context) {
        if (context.stop_requested()) {
            return contracts::PatternLearningResult::failed(
                "pattern_learning.observe", "cancelled",
                "cycle invocation was cancelled");
        }
        return observe(observation);
    }
};

}  // namespace eu_digital
