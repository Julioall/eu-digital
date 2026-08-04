#pragma once

#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/port_result.hpp"
#include "core/contracts/workspace_snapshot.hpp"
#include "core/contracts/metacognitive_assessment.hpp"

#include <stdexcept>

namespace eu_digital {

class IMetacognitionPort {
public:
    virtual ~IMetacognitionPort() = default;

    virtual contracts::MetacognitiveAssessment evaluate(const contracts::WorkspaceSnapshot& workspace) = 0;

    virtual contracts::MetacognitivePortAssessment evaluate_hypothesis(
        const contracts::MetacognitionRequest&) {
        throw std::logic_error("metacognition requests are not implemented");
    }

    contracts::PortResult<contracts::MetacognitiveAssessment> evaluate_result(
        const contracts::WorkspaceSnapshot& workspace) {
        return contracts::capture_port_result<contracts::MetacognitiveAssessment>(
            "metacognition.evaluate", [&] { return evaluate(workspace); });
    }

    contracts::PortResult<contracts::MetacognitivePortAssessment>
    evaluate_hypothesis_result(const contracts::MetacognitionRequest& request) {
        return contracts::capture_port_result<contracts::MetacognitivePortAssessment>(
            "metacognition.evaluate_hypothesis",
            [&] { return evaluate_hypothesis(request); });
    }
};

} // namespace eu_digital
