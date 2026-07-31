#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/episode_update.hpp"

namespace eu_digital {

class IEpisodeBoundaryPort {
public:
    virtual ~IEpisodeBoundaryPort() = default;

    virtual EpisodeUpdate evaluate(const CanonicalEvent& event) = 0;
};

} // namespace eu_digital
