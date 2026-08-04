#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/episode_update.hpp"
#include "core/contracts/port_result.hpp"

namespace eu_digital {

class IEpisodeBoundaryPort {
public:
    virtual ~IEpisodeBoundaryPort() = default;

    virtual EpisodeUpdate evaluate(const CanonicalEvent& event) = 0;

    contracts::PortResult<EpisodeUpdate> evaluate_result(const CanonicalEvent& event) {
        return contracts::capture_port_result<EpisodeUpdate>(
            "episode_boundary.evaluate", [&] { return evaluate(event); });
    }
};

} // namespace eu_digital
