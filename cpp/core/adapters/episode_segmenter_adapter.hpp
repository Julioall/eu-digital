#pragma once

#include "core/episode_segmenter.hpp"
#include "core/ports/iepisode_boundary_port.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace eu_digital {

class EpisodeSegmenterAdapter final : public IEpisodeBoundaryPort {
public:
    EpisodeSegmenterAdapter() = default;

    EpisodeUpdate evaluate(const CanonicalEvent&) override {
        throw std::invalid_argument(
            "legacy canonical event lacks episode observation fields");
    }

    EpisodeUpdate evaluate_observation(
        const contracts::EpisodeObservationRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid episode observation request");
        }
        std::lock_guard lock(mutex_);

        EpisodeSegmentEvent event;
        event.event_id = request.event_id;
        event.session_id = request.session_id;
        event.occurred_at = request.occurred_at;
        event.epoch_seconds = request.epoch_seconds;
        event.application = request.application;
        event.document = request.document;
        event.modality = request.modality;

        auto& events = events_by_session_[request.session_id];
        events.push_back(std::move(event));
        const auto result = EpisodeSegmenter::segment(events);

        EpisodeUpdate update;
        update.episode_id = result.episodes.back().episode_id;
        update.is_new_episode = std::any_of(
            result.boundaries.begin(), result.boundaries.end(),
            [&](const auto& boundary) {
                return boundary.event_id == request.event_id;
            });
        update.current_state = "active";
        return update;
    }

private:
    std::mutex mutex_;
    std::map<std::string, std::vector<EpisodeSegmentEvent>> events_by_session_;
};

}  // namespace eu_digital
