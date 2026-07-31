#pragma once

#include "core/event_bus.hpp"
#include <string>
#include <vector>

namespace eu_digital {

struct WorkspaceSnapshot {
    std::string workspace_id;
    std::vector<CanonicalEvent> active_events;
    double tension;

    bool valid() const {
        return !workspace_id.empty();
    }
};

} // namespace eu_digital
