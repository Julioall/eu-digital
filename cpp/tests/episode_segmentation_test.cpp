#include "core/capability_runtime.hpp"
#include "core/episode_segmenter.hpp"

#include <cassert>
#include <string>
#include <vector>

using eu_digital::CapabilityRegistry;
using eu_digital::CapabilityState;
using eu_digital::EpisodeSegmentConfig;
using eu_digital::EpisodeSegmentEvent;
using eu_digital::EpisodeSegmentationPlugin;
using eu_digital::ModuleLifecycleManager;

EpisodeSegmentEvent event(std::string id, double time, std::string app, std::string document = {}) {
    EpisodeSegmentEvent value;
    value.event_id = std::move(id);
    value.session_id = "session-test";
    value.occurred_at = "2026-01-01T00:00:0" + std::to_string(static_cast<int>(time)) + "Z";
    value.epoch_seconds = time;
    if (!app.empty()) value.application = std::move(app);
    if (!document.empty()) value.document = std::move(document);
    value.modality = "system";
    return value;
}

int main() {
    const std::vector<EpisodeSegmentEvent> events{
        event("e1", 0.0, "editor", "a.txt"),
        event("e2", 1.0, "editor", "a.txt"),
        event("e3", 2.0, "browser", "a.txt"),
        event("e4", 3.0, "browser", "b.html"),
    };
    const auto result = eu_digital::EpisodeSegmenter::segment(events, EpisodeSegmentConfig{3.0, true, true});
    assert(result.episodes.size() == 3);
    assert(result.boundaries.size() == 3);
    assert(result.boundaries[1].reasons[0] == "context_change:application");
    assert(result.boundaries[2].reasons[0] == "context_change:document");
    assert(result.episodes[0].event_ids.size() == 2);
    assert(result.episodes[0].applications[0] == "editor");
    assert(result.episodes[0].documents[0] == "a.txt");
    assert(result.episodes[0].episode_id == eu_digital::EpisodeSegmenter::segment(events, EpisodeSegmentConfig{3.0, true, true}).episodes[0].episode_id);

    auto missing = events;
    missing[1].application.reset();
    missing[1].document.reset();
    const auto missing_result = eu_digital::EpisodeSegmenter::segment(missing, EpisodeSegmentConfig{30.0, true, true});
    assert(missing_result.episodes.size() == 3);

    const auto ablated = eu_digital::EpisodeSegmenter::segment(events, EpisodeSegmentConfig{3.0, false, false});
    assert(ablated.episodes.size() == 1);

    auto invalid = events;
    invalid[2].epoch_seconds = -1.0;
    try {
        eu_digital::EpisodeSegmenter::segment(invalid);
        assert(false);
    } catch (const std::invalid_argument&) {
    }

    CapabilityRegistry registry;
    ModuleLifecycleManager lifecycle(registry);
    EpisodeSegmentationPlugin plugin;
    assert(lifecycle.install(plugin));
    assert(registry.record("native.episode_segmenter").state.state == CapabilityState::available);
    lifecycle.remove("native.episode_segmenter");
    assert(registry.record("native.episode_segmenter").state.state == CapabilityState::removed);
}
