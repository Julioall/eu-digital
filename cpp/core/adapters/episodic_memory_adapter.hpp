#pragma once

#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/episodic_memory.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <string>

namespace eu_digital {

class EpisodicMemoryAdapter final : public IMemoryWritePort, public IMemoryRetrievalPort {
public:
    explicit EpisodicMemoryAdapter(std::shared_ptr<EpisodicMemoryStore> store)
        : store_(std::move(store)) {
        if (!store_) {
            throw std::invalid_argument("store cannot be null");
        }
    }

    MemoryWriteResult store_event(const CanonicalEvent& event) override {
        // IMemoryWritePort no design da SPEC-045 espera que a escrita 
        // interaja indiretamente através do envio do evento.
        // No entanto, EpisodicMemoryStore recebe um MemoryEpisode.
        // Precisamos converter o CanonicalEvent pra algo ou ignorar.
        // Em um sistema real, o coordenador faria a ligação:
        // Segmenter(Event) -> Boundary -> se Boundary, gera Episodio -> Store(Episode).
        // Por hora, apenas retornamos sucesso para validar as abstrações.
        std::lock_guard lock(mutex_);
        
        // Simulação do armazenamento
        return MemoryWriteResult::ok(event.event_id + "-mem");
    }

    RetrievedMemorySet retrieve(const std::string& query, int limit = 5) override {
        std::lock_guard lock(mutex_);
        MemoryQuery mem_query;
        // Na prática, a query textual seria convertida em MemoryQuery ou embeddings.
        mem_query.limit = limit;
        auto results = store_->retrieve(mem_query);

        RetrievedMemorySet res;
        for (const auto& r : results) {
            RetrievedMemorySet::MemoryItem item;
            item.memory_id = r.episode.episode_id;
            item.payload = "{}"; // JSON seria formatado aqui
            item.relevance = r.score;
            res.items.push_back(std::move(item));
        }
        return res;
    }

private:
    std::shared_ptr<EpisodicMemoryStore> store_;
    std::mutex mutex_;
};

} // namespace eu_digital
