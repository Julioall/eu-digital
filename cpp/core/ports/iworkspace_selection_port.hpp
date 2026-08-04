#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/port_result.hpp"
#include "core/contracts/workspace_snapshot.hpp"

namespace eu_digital {

class IWorkspaceSelectionPort {
public:
    virtual ~IWorkspaceSelectionPort() = default;

    virtual contracts::WorkspaceSnapshot select(const CanonicalEvent& event) = 0;

    contracts::PortResult<contracts::WorkspaceSnapshot> select_result(
        const CanonicalEvent& event) {
        return contracts::capture_port_result<contracts::WorkspaceSnapshot>(
            "workspace.select", [&] { return select(event); });
    }
};

} // namespace eu_digital
