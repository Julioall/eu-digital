#pragma once

#include <string>

namespace eu_digital {

struct EpisodeUpdate {
    std::string episode_id;
    bool is_new_episode;
    std::string current_state;

    bool valid() const {
        return !episode_id.empty() && !current_state.empty();
    }
};

} // namespace eu_digital
