#pragma once

#include "core/ports/iprediction_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"
#include "core/ports/iself_model_query_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/icognitive_decision_port.hpp"
#include "core/ports/ipattern_learning_port.hpp"

#include "core/adapters/world_model_adapter.hpp"
#include "core/adapters/episodic_memory_adapter.hpp"
#include "core/adapters/episode_segmenter_adapter.hpp"
#include "core/adapters/global_workspace_adapter.hpp"
#include "core/adapters/functional_self_model_adapter.hpp"
#include "core/adapters/metacognition_curiosity_adapter.hpp"
#include "core/adapters/cognitive_decision_adapter.hpp"
#include "core/adapters/pattern_learner_adapter.hpp"

#include <memory>
#include <stdexcept>

namespace eu_digital {

// Factory que converte instâncias concretas do sistema legado em Portas Virtuais (Abstrações)
class CognitivePortFactory {
public:
    static std::shared_ptr<IPredictionPort> create_prediction_port(std::shared_ptr<WorldModel> wm) {
        if (!wm) throw std::invalid_argument("WorldModel is required");
        return std::make_shared<WorldModelAdapter>(std::move(wm));
    }

    static std::shared_ptr<IMemoryWritePort> create_memory_write_port(std::shared_ptr<EpisodicMemoryStore> store) {
        if (!store) throw std::invalid_argument("EpisodicMemoryStore is required");
        return std::make_shared<EpisodicMemoryAdapter>(store);
    }

    static std::shared_ptr<IMemoryRetrievalPort> create_memory_retrieval_port(std::shared_ptr<EpisodicMemoryStore> store) {
        if (!store) throw std::invalid_argument("EpisodicMemoryStore is required");
        return std::make_shared<EpisodicMemoryAdapter>(store);
    }

    static std::shared_ptr<IEpisodeBoundaryPort> create_episode_boundary_port() {
        return std::make_shared<EpisodeSegmenterAdapter>();
    }

    static std::shared_ptr<IWorkspaceSelectionPort> create_workspace_selection_port(std::shared_ptr<GlobalWorkspace> ws) {
        if (!ws) throw std::invalid_argument("GlobalWorkspace is required");
        return std::make_shared<GlobalWorkspaceAdapter>(std::move(ws));
    }

    static std::shared_ptr<ISelfModelQueryPort> create_self_model_query_port(std::shared_ptr<VersionedFunctionalSelfModel> sm) {
        if (!sm) throw std::invalid_argument("VersionedFunctionalSelfModel is required");
        return std::make_shared<FunctionalSelfModelAdapter>(std::move(sm));
    }

    static std::shared_ptr<IMetacognitionPort> create_metacognition_port(std::shared_ptr<MetacognitionCuriosityEngine> mc) {
        if (!mc) throw std::invalid_argument("MetacognitionCuriosityEngine is required");
        return std::make_shared<MetacognitionCuriosityAdapter>(std::move(mc));
    }

    static std::shared_ptr<ICognitiveDecisionPort> create_cognitive_decision_port(std::shared_ptr<SuggestionOrchestrator> so) {
        if (!so) throw std::invalid_argument("SuggestionOrchestrator is required");
        return std::make_shared<CognitiveDecisionAdapter>(std::move(so));
    }

    static std::shared_ptr<IPatternLearningPort> create_pattern_learning_port(
        std::shared_ptr<PatternLearner> learner) {
        if (!learner) throw std::invalid_argument("PatternLearner is required");
        return std::make_shared<PatternLearnerAdapter>(std::move(learner));
    }
};

} // namespace eu_digital
