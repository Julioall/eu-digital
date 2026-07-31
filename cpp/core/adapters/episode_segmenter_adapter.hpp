#pragma once

#include "core/ports/iepisode_boundary_port.hpp"
#include "core/episode_segmenter.hpp"
#include <mutex>
#include <vector>
#include <string>

namespace eu_digital {

class EpisodeSegmenterAdapter final : public IEpisodeBoundaryPort {
public:
    EpisodeSegmenterAdapter() = default;

    EpisodeUpdate evaluate(const CanonicalEvent& event) override {
        std::lock_guard lock(mutex_);
        
        EpisodeSegmentEvent seg_ev;
        seg_ev.event_id = event.event_id;
        seg_ev.session_id = "default-session";
        seg_ev.occurred_at = "now";
        seg_ev.epoch_seconds = static_cast<double>(event.monotonic_ns) / 1e9;
        
        events_buffer_.push_back(seg_ev);
        
        // Simplesmente avalia o buffer atual usando o segmenter estático
        auto result = EpisodeSegmenter::segment(events_buffer_);
        
        EpisodeUpdate update;
        update.episode_id = result.episodes.empty() ? "pending" : result.episodes.back().episode_id;
        update.is_new_episode = !result.boundaries.empty();
        update.current_state = "active";
        
        return update;
    }

private:
    std::mutex mutex_;
    std::vector<EpisodeSegmentEvent> events_buffer_;
};

} // namespace eu_digital
