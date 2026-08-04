#pragma once

#include "core/contracts/cognitive_output.hpp"

namespace eu_digital {

inline constexpr char kLanguageRenderOperation[] = "language.render";

class ILanguageRenderer {
public:
    virtual ~ILanguageRenderer() = default;

    virtual contracts::ValidatedDialogueOutputV1 render(
        const contracts::CognitiveOutputRequestV1& request) = 0;
};

}  // namespace eu_digital
