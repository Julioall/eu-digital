#pragma once

#include "core/ports/iprediction_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/iepisode_boundary_port.hpp"

#include "core/adapters/world_model_adapter.hpp"
#include "core/adapters/episodic_memory_adapter.hpp"
#include "core/adapters/episode_segmenter_adapter.hpp"

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
};

} // namespace eu_digital
