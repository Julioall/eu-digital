#include "core/capability_runtime.hpp"
#include "core/episodic_memory.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::EpisodicMemoryPlugin;
using eu_digital::EpisodicMemoryStore;
using eu_digital::MemoryEpisode;
using eu_digital::MemoryQuery;
using eu_digital::ModuleLifecycleManager;

MemoryEpisode episode(std::string id, std::string application, double start) {
    MemoryEpisode value;
    value.episode_id = std::move(id);
    value.session_id = "session-test";
    value.start_at = "2026-01-01T00:00:00Z";
    value.end_at = "2026-01-01T00:00:10Z";
    value.start_epoch = start;
    value.end_epoch = start + 10.0;
    value.event_ids = {value.episode_id + "-event"};
    value.applications = {application};
    value.modalities = {application == "browser" ? "input" : "system"};
    value.boundary_reasons = {"episode_start"};
    value.created_by = "test";
    return value;
}

int main() {
    bool invalid_episode_rejected = false;
    try {
        EpisodicMemoryStore().store(MemoryEpisode{});
    } catch (const std::invalid_argument&) {
        invalid_episode_rejected = true;
    }
    assert(invalid_episode_rejected);

    EpisodicMemoryStore memory(2);
    assert(memory.store(episode("ep-1", "editor", 0.0)) == "accepted");
    assert(memory.store(episode("ep-1", "mutated", 0.0)) == "duplicate");
    assert(memory.store(episode("ep-2", "browser", 20.0), std::vector<double>{0.0, 1.0}) == "accepted");
    assert(memory.store(episode("ep-3", "editor", 40.0), std::vector<double>{1.0, 0.0}) == "accepted");

    MemoryQuery context;
    context.applications = {"editor"};
    assert(memory.retrieve(context).size() == 2);
    assert(memory.retrieve(context).front().episode.episode_id == "ep-1");
    assert(memory.retrieve(context).front().reason_codes.front() == "context.application");

    MemoryQuery embedding;
    embedding.embedding = {0.9, 0.1};
    assert(memory.retrieve(embedding).front().episode.episode_id == "ep-3");
    assert(memory.similarity_relations(0.3).size() == 1);

    bool empty_embedding_rejected = false;
    try {
        MemoryQuery invalid_query;
        invalid_query.embedding = std::vector<double>{};
        memory.retrieve(invalid_query);
    } catch (const std::invalid_argument&) {
        empty_embedding_rejected = true;
    }
    assert(empty_embedding_rejected);

    const auto removed = memory.consolidate();
    assert(removed.size() == 1);
    assert(!memory.contains("ep-1"));
    assert(memory.size() == 2);

    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    EpisodicMemoryPlugin plugin;
    assert(lifecycle.install(plugin));
    assert(registry.record("native.episodic_memory").state.state == CapabilityState::available);
    lifecycle.remove("native.episodic_memory");
    assert(registry.record("native.episodic_memory").state.state == CapabilityState::removed);
}
