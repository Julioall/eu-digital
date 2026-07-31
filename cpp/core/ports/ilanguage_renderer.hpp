#pragma once

#include "core/contracts/cognitive_output.hpp"

namespace eu_digital {

class ILanguageRenderer {
public:
    virtual ~ILanguageRenderer() = default;

    virtual ValidatedDialogueOutput render(const CognitiveOutputRequest& request) = 0;
};

} // namespace eu_digital
