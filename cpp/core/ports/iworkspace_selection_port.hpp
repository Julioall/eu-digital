#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/workspace_snapshot.hpp"

namespace eu_digital {

class IWorkspaceSelectionPort {
public:
    virtual ~IWorkspaceSelectionPort() = default;

    virtual contracts::WorkspaceSnapshot select(const CanonicalEvent& event) = 0;
};

} // namespace eu_digital
