#pragma once

#include "core/contracts/cognitive_output.hpp"

namespace eu_digital {

class IPresentationPort {
public:
    virtual ~IPresentationPort() = default;

    virtual void present(const ValidatedDialogueOutput& output) = 0;
};

} // namespace eu_digital
