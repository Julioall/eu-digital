#pragma once

#include "core/contracts/cognitive_output.hpp"
#include "core/contracts/port_result.hpp"

namespace eu_digital {

inline constexpr char kPresentationOperation[] = "presentation.present";

class IPresentationPort {
public:
    virtual ~IPresentationPort() = default;

    virtual contracts::PortResult<bool> present(
        const contracts::ValidatedDialogueOutputV1& output) = 0;
};

}  // namespace eu_digital
