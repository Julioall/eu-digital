#include "core/global_workspace.hpp"

#include <cassert>
#include <stdexcept>

namespace {

eu_digital::WorkspaceCandidate candidate(const std::string& id, const std::string& session, double novelty) {
    eu_digital::WorkspaceCandidate value;
    value.candidate_id = id;
    value.session_id = session;
    value.source_kind = "canonical_event";
    value.source_refs = {"event:" + id};
    value.observed_at = "2026-01-01T00:00:00+00:00";
    value.content = {{"kind", "\"observation\""}};
    value.salience_signals = {{"novelty", novelty}};
    return value;
}

void selection_and_lifecycle_are_bounded() {
    eu_digital::WorkspaceConfig config;
    config.capacity = 1;
    config.max_candidates = 2;
    config.ttl_seconds = 10.0;
    eu_digital::GlobalWorkspace workspace("workspace-test", "session-test", config);

    const auto first = workspace.admit(candidate("candidate-a", "session-test", 0.2), "2026-01-01T00:00:00+00:00", 1767225600.0);
    assert(first.active_items.size() == 1);
    assert(first.active_items.front().candidate_id == "candidate-a");
    const auto second = workspace.admit(candidate("candidate-b", "session-test", 0.9), "2026-01-01T00:00:01+00:00", 1767225601.0);
    assert(second.active_items.size() == 1);
    assert(second.active_items.front().candidate_id == "candidate-b");
    assert(second.decisions.size() == 2);
    assert(second.decisions.front().candidate_id == "candidate-b");

    const auto expired = workspace.snapshot("2026-01-01T00:00:11+00:00", 1767225611.0);
    assert(expired.expired_candidate_ids.size() == 2);
    assert(expired.active_items.empty());
}

void fifo_and_capability_contract_are_explicit() {
    eu_digital::WorkspaceConfig config;
    config.capacity = 1;
    config.max_candidates = 2;
    config.selection_policy = eu_digital::WORKSPACE_BASELINE_ID;
    eu_digital::GlobalWorkspace workspace("workspace-fifo", "session-test", config);
    const auto first = workspace.admit(candidate("candidate-z", "session-test", 0.9), "2026-01-01T00:00:00+00:00", 1767225600.0);
    const auto second = workspace.admit(candidate("candidate-a", "session-test", 0.1), "2026-01-01T00:00:01+00:00", 1767225601.0);
    assert(first.active_items.front().candidate_id == "candidate-z");
    assert(second.active_items.front().candidate_id == "candidate-z");
    assert(second.active_items.front().selection_reasons.back() == "selection.fifo_admission");

    eu_digital::GlobalWorkspacePlugin plugin;
    assert(plugin.descriptor().valid());
    assert(plugin.descriptor().provides_operation("select.workspace"));
    assert(plugin.descriptor().supports_hot_plug);
}

void invalid_inputs_are_rejected() {
    auto invalid = candidate("candidate-invalid", "wrong-session", 0.5);
    eu_digital::GlobalWorkspace workspace("workspace-test", "session-test", {});
    bool rejected = false;
    try {
        workspace.admit(std::move(invalid), "2026-01-01T00:00:00+00:00", 1767225600.0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

}  // namespace

int main() {
    selection_and_lifecycle_are_bounded();
    fifo_and_capability_contract_are_explicit();
    invalid_inputs_are_rejected();
    return 0;
}
