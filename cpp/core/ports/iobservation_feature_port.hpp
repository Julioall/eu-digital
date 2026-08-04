#pragma once

#include "core/contracts/cognitive_cycle_v1.hpp"

namespace eu_digital {

class IObservationFeaturePort {
public:
    virtual ~IObservationFeaturePort() = default;

    virtual contracts::PortResult<contracts::ObservationFeaturesV1> extract_features(
        const contracts::CognitiveCycleInputV1& input,
        const contracts::PortInvocationContextV1& context) = 0;
};

}  // namespace eu_digital
