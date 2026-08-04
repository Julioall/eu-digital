#include "core/adapters/episodic_memory_adapter.hpp"
#include "core/episodic_memory.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace eu_digital;

void test_episodic_memory_adapter() {
    std::cout << "Starting test_episodic_memory_adapter" << std::endl;
    auto store = std::make_shared<EpisodicMemoryStore>();
    EpisodicMemoryAdapter adapter(store);
    
    contracts::EpisodeWriteRequest request;
    request.episode.episode_id = "episode-1";
    request.episode.session_id = "session-1";
    request.episode.start_at = "2026-08-04T12:00:00Z";
    request.episode.end_at = "2026-08-04T12:00:01Z";
    request.episode.event_ids = {"event-1"};
    request.episode.applications = {"editor"};
    request.episode.documents = {"spec.md"};
    request.episode.modalities = {"system_activity"};
    request.episode.boundary_reasons = {"episode_start"};
    request.episode.created_by = "adapter-test";
    request.start_epoch = 1.0;
    request.end_epoch = 2.0;
    request.embedding = std::vector<double>{1.0, 0.0};
    auto write_envelope = adapter.store_episode_result(request);
    
    if (!write_envelope.valid() || !write_envelope.success ||
        !write_envelope.value || !write_envelope.value->success) {
        throw std::runtime_error("Write failed");
    }
    
    if (!store->contains("episode-1")) {
        throw std::runtime_error("Episode was not delegated to store");
    }

    contracts::MemoryRetrievalRequest query;
    query.session_id = "session-1";
    query.applications = {"editor"};
    query.documents = {"spec.md"};
    query.modalities = {"system_activity"};
    query.start_epoch = 1.0;
    query.end_epoch = 2.0;
    query.embedding = std::vector<double>{1.0, 0.0};
    query.limit = 5;
    auto retrieve_envelope = adapter.retrieve_memory_result(query);
    if (!retrieve_envelope.valid() || !retrieve_envelope.success ||
        !retrieve_envelope.value) {
        throw std::runtime_error("Retrieve failed");
    }
    const auto& retrieve_res = *retrieve_envelope.value;
    if (!retrieve_res.valid() || retrieve_res.items.size() != 1 ||
        retrieve_res.items.front().memory_id != "episode-1" ||
        std::abs(retrieve_res.items.front().relevance - 2.0) > 1e-12 ||
        retrieve_res.items.front().session_id != "session-1" ||
        retrieve_res.items.front().event_ids != std::vector<std::string>{"event-1"} ||
        retrieve_res.items.front().applications != std::vector<std::string>{"editor"} ||
        retrieve_res.items.front().documents != std::vector<std::string>{"spec.md"} ||
        retrieve_res.items.front().modalities != std::vector<std::string>{"system_activity"}) {
        throw std::runtime_error("Expected faithful memory retrieval mapping");
    }

    contracts::MemoryRetrievalRequest invalid_query;
    invalid_query.limit = 0;
    if (adapter.retrieve_memory_result(invalid_query).success) {
        throw std::runtime_error("Expected invalid structured query rejection");
    }

    CanonicalEvent legacy;
    legacy.event_id = "legacy";
    if (adapter.store_event_result(legacy).success) {
        throw std::runtime_error("Expected legacy write rejection");
    }
}

int main() {
    try {
        test_episodic_memory_adapter();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
