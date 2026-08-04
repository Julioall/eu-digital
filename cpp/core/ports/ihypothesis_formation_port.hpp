#pragma once

#include "core/contracts/cognitive_cycle_v1.hpp"

namespace eu_digital {

class IHypothesisFormationPort {
public:
    virtual ~IHypothesisFormationPort() = default;

    virtual contracts::PortResult<contracts::HypothesisFormationResultV1>
    form_hypothesis(
        const contracts::HypothesisFormationRequestV1& request,
        const contracts::PortInvocationContextV1& context) = 0;
};

}  // namespace eu_digital
