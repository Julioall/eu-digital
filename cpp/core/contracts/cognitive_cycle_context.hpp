#pragma once

#include "core/contracts/episode_update.hpp"
#include "core/contracts/memory_write_result.hpp"
#include "core/contracts/prediction_assessment.hpp"
#include "core/contracts/workspace_snapshot.hpp"
#include "core/contracts/metacognitive_assessment.hpp"
#include "core/contracts/self_constraint_snapshot.hpp"
#include "core/contracts/cognitive_decision.hpp"

#include <optional>
#include <string>

namespace eu_digital {

/// Aggregates the intermediate results of a single cognitive cycle pass.
/// Each stage populates its field so subsequent stages can use prior results.
struct CognitiveCycleContext {
    std::optional<EpisodeUpdate> episode;
    std::optional<MemoryWriteResult> memory;
    std::optional<PredictionAssessment> prediction;
    contracts::WorkspaceSnapshot workspace;
    std::optional<contracts::MetacognitiveAssessment> metacognition;
    std::optional<SelfConstraintSnapshot> self_model;
    std::optional<CognitiveDecision> decision;

    std::string degradation_reason;
    bool degraded = false;

    void mark_degraded(const std::string& reason) {
        degraded = true;
        if (!degradation_reason.empty()) degradation_reason += "; ";
        degradation_reason += reason;
    }
};

} // namespace eu_digital
