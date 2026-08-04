#pragma once

#include "core/contracts/cognitive_cycle_v1.hpp"

namespace eu_digital {

class ISalienceAssessmentPort {
public:
    virtual ~ISalienceAssessmentPort() = default;

    virtual contracts::PortResult<contracts::SalienceAssessmentV1> assess_salience(
        const contracts::SalienceAssessmentRequestV1& request,
        const contracts::PortInvocationContextV1& context) = 0;
};

}  // namespace eu_digital
