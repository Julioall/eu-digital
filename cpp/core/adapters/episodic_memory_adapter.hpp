#pragma once

#include "core/episodic_memory.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/imemory_write_port.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace eu_digital {

class EpisodicMemoryAdapter final : public IMemoryWritePort,
                                    public IMemoryRetrievalPort {
public:
    explicit EpisodicMemoryAdapter(std::shared_ptr<EpisodicMemoryStore> store)
        : store_(std::move(store)) {
        if (!store_) throw std::invalid_argument("store cannot be null");
    }

    MemoryWriteResult store_event(const CanonicalEvent&) override {
        throw std::invalid_argument(
            "legacy canonical event cannot represent a memory episode");
    }

    MemoryWriteResult store_episode(
        const contracts::EpisodeWriteRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid episode write request");
        }
        std::lock_guard lock(mutex_);

        MemoryEpisode episode;
        episode.episode_id = request.episode.episode_id;
        episode.schema_version = request.episode.schema_version;
        episode.session_id = request.episode.session_id;
        episode.start_at = request.episode.start_at;
        episode.end_at = request.episode.end_at;
        episode.start_epoch = request.start_epoch;
        episode.end_epoch = request.end_epoch;
        episode.event_ids = request.episode.event_ids;
        episode.applications = request.episode.applications;
        episode.documents = request.episode.documents;
        episode.people = request.episode.people;
        episode.topics = request.episode.topics;
        episode.modalities = request.episode.modalities;
        episode.boundary_reasons = request.episode.boundary_reasons;
        episode.embedding_ref = request.episode.embedding_ref;
        episode.summary = request.episode.summary;
        episode.hypotheses = request.episode.hypotheses;
        episode.coherence = request.episode.coherence;
        episode.confidence = request.episode.confidence;
        episode.created_by = request.episode.created_by;

        const auto status = store_->store(std::move(episode), request.embedding);
        if (status != "accepted" && status != "duplicate") {
            return MemoryWriteResult::fail("memory store rejected episode");
        }
        return MemoryWriteResult::ok(request.episode.episode_id);
    }

    RetrievedMemorySet retrieve(const std::string&, int = 5) override {
        throw std::invalid_argument(
            "legacy text query cannot represent a structured memory query");
    }

    contracts::MemoryRetrievalResponse retrieve_memory(
        const contracts::MemoryRetrievalRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid memory retrieval request");
        }
        std::lock_guard lock(mutex_);

        MemoryQuery query;
        query.session_id = request.session_id;
        query.applications = request.applications;
        query.documents = request.documents;
        query.modalities = request.modalities;
        query.start_epoch = request.start_epoch;
        query.end_epoch = request.end_epoch;
        query.embedding = request.embedding;
        query.limit = request.limit;
        const auto results = store_->retrieve(query);

        contracts::MemoryRetrievalResponse response;
        for (const auto& result : results) {
            contracts::MemoryRetrievalItem item;
            item.memory_id = result.episode.episode_id;
            item.relevance = result.score;
            item.session_id = result.episode.session_id;
            item.event_ids = result.episode.event_ids;
            item.applications = result.episode.applications;
            item.documents = result.episode.documents;
            item.modalities = result.episode.modalities;
            item.reason_codes = result.reason_codes;
            response.items.push_back(std::move(item));
        }
        return response;
    }

private:
    std::shared_ptr<EpisodicMemoryStore> store_;
    std::mutex mutex_;
};

}  // namespace eu_digital
