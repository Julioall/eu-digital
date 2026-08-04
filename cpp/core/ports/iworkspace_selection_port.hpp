#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/port_result.hpp"
#include "core/contracts/workspace_snapshot.hpp"

#include <stdexcept>

namespace eu_digital {

class IWorkspaceSelectionPort {
public:
    virtual ~IWorkspaceSelectionPort() = default;

    virtual contracts::WorkspaceSnapshot select(const CanonicalEvent& event) = 0;

    virtual contracts::WorkspaceAssessment select_candidate(
        const contracts::WorkspaceSelectionRequest&) {
        throw std::logic_error("workspace selection requests are not implemented");
    }

    contracts::PortResult<contracts::WorkspaceSnapshot> select_result(
        const CanonicalEvent& event) {
        return contracts::capture_port_result<contracts::WorkspaceSnapshot>(
            "workspace.select", [&] { return select(event); });
    }

    contracts::PortResult<contracts::WorkspaceAssessment> select_candidate_result(
        const contracts::WorkspaceSelectionRequest& request) {
        return contracts::capture_port_result<contracts::WorkspaceAssessment>(
            "workspace.select_candidate", [&] { return select_candidate(request); });
    }

    virtual contracts::PortResult<contracts::WorkspaceAssessment>
    select_candidate_context(
        const contracts::WorkspaceSelectionRequest& request,
        const contracts::PortInvocationContextV1& context) {
        if (context.stop_requested()) {
            return contracts::PortResult<contracts::WorkspaceAssessment>::failed(
                "workspace.select_candidate", "cancelled",
                "cycle invocation was cancelled");
        }
        return select_candidate_result(request);
    }
};

} // namespace eu_digital
