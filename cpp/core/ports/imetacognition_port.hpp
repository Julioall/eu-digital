#pragma once

#include "core/contracts/workspace_snapshot.hpp"
#include "core/contracts/metacognitive_assessment.hpp"

namespace eu_digital {

class IMetacognitionPort {
public:
    virtual ~IMetacognitionPort() = default;

    virtual contracts::MetacognitiveAssessment evaluate(const contracts::WorkspaceSnapshot& workspace) = 0;
};

} // namespace eu_digital
