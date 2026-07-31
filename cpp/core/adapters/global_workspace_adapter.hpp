#pragma once

#include "core/ports/iworkspace_selection_port.hpp"
#include "core/global_workspace.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class GlobalWorkspaceAdapter final : public IWorkspaceSelectionPort {
public:
    explicit GlobalWorkspaceAdapter(std::shared_ptr<GlobalWorkspace> workspace)
        : workspace_(std::move(workspace)) {
        if (!workspace_) {
            throw std::invalid_argument("workspace cannot be null");
        }
    }

    contracts::WorkspaceSnapshot select(const CanonicalEvent& event) override {
        std::lock_guard lock(mutex_);
        // Snapshot the workspace internal state
        auto internal_snap = workspace_->snapshot("", static_cast<double>(event.monotonic_ns));
        
        // Map to abstraction
        contracts::WorkspaceSnapshot result;
        result.workspace_id = internal_snap.workspace_id;
        result.tension = 0.5; // dummy
        return result;
    }

private:
    std::shared_ptr<GlobalWorkspace> workspace_;
    std::mutex mutex_;
};

} // namespace eu_digital
