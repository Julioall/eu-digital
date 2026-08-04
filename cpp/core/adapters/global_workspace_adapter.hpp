#pragma once

#include "core/global_workspace.hpp"
#include "core/ports/iworkspace_selection_port.hpp"

#include <memory>
#include <mutex>
#include <stdexcept>

namespace eu_digital {

class GlobalWorkspaceAdapter final : public IWorkspaceSelectionPort {
public:
    explicit GlobalWorkspaceAdapter(std::shared_ptr<GlobalWorkspace> workspace)
        : workspace_(std::move(workspace)) {
        if (!workspace_) throw std::invalid_argument("workspace cannot be null");
    }

    contracts::WorkspaceSnapshot select(const CanonicalEvent&) override {
        throw std::invalid_argument(
            "legacy canonical event lacks workspace candidate fields");
    }

    contracts::WorkspaceAssessment select_candidate(
        const contracts::WorkspaceSelectionRequest& request) override {
        if (!request.valid()) {
            throw std::invalid_argument("invalid workspace selection request");
        }
        std::lock_guard lock(mutex_);

        WorkspaceCandidate candidate;
        candidate.candidate_id = request.candidate_id;
        candidate.session_id = request.session_id;
        candidate.source_kind = request.source_kind;
        candidate.source_refs = request.source_refs;
        candidate.observed_at = request.observed_at;
        candidate.content = request.content;
        candidate.salience_signals = request.salience_signals;
        candidate.schema_version = request.schema_version;
        const auto snapshot = workspace_->admit(
            std::move(candidate), request.observed_at, request.observed_epoch);

        contracts::WorkspaceAssessment result;
        result.schema_version = snapshot.schema_version;
        result.snapshot_id = snapshot.snapshot_id;
        result.workspace_id = snapshot.workspace_id;
        result.session_id = snapshot.session_id;
        result.created_at = snapshot.created_at;
        result.capacity = snapshot.capacity;
        result.policy_id = snapshot.policy_id;
        result.config_fingerprint = snapshot.config_fingerprint;
        result.selection_churn = snapshot.selection_churn;
        for (const auto& item : snapshot.active_items) {
            result.active_candidate_ids.push_back(item.candidate_id);
        }
        result.expired_candidate_ids = snapshot.expired_candidate_ids;
        result.discarded_candidate_ids = snapshot.discarded_candidate_ids;
        return result;
    }

private:
    std::shared_ptr<GlobalWorkspace> workspace_;
    std::mutex mutex_;
};

}  // namespace eu_digital
