#pragma once

#include "core/contracts/port_result.hpp"
#include "core/contracts/workspace_snapshot.hpp"
#include "core/contracts/metacognitive_assessment.hpp"

namespace eu_digital {

class IMetacognitionPort {
public:
    virtual ~IMetacognitionPort() = default;

    virtual contracts::MetacognitiveAssessment evaluate(const contracts::WorkspaceSnapshot& workspace) = 0;

    contracts::PortResult<contracts::MetacognitiveAssessment> evaluate_result(
        const contracts::WorkspaceSnapshot& workspace) {
        return contracts::capture_port_result<contracts::MetacognitiveAssessment>(
            "metacognition.evaluate", [&] { return evaluate(workspace); });
    }
};

} // namespace eu_digital
