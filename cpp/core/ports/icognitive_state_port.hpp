#pragma once

#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/cognitive_state_v1.hpp"
#include "core/contracts/port_result.hpp"

#include <string>

namespace eu_digital {

class ICognitiveStatePort {
public:
    virtual ~ICognitiveStatePort() = default;

    virtual std::string provider_id() const = 0;
    virtual std::string state_schema_version() const = 0;

    virtual contracts::PortResult<contracts::CognitiveStateFragmentV1>
    capture_state(const contracts::PortInvocationContextV1& context) const = 0;

    virtual contracts::PortResult<contracts::CognitiveStateRestoreResultV1>
    restore_state(const contracts::CognitiveStateFragmentV1& fragment,
                  const contracts::PortInvocationContextV1& context) = 0;
};

}  // namespace eu_digital
