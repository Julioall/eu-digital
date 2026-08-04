#include "core/adapters/global_workspace_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto ws = std::make_shared<GlobalWorkspace>("ws1", "session1", WorkspaceConfig{});
        GlobalWorkspaceAdapter adapter(ws);
        
        contracts::WorkspaceSelectionRequest request;
        request.candidate_id = "candidate-1";
        request.session_id = "session1";
        request.source_kind = "canonical_event";
        request.source_refs = {"event-1"};
        request.observed_at = "2026-08-04T12:00:00Z";
        request.observed_epoch = 1.0;
        request.content = {{"event_type", "system_observation"}};
        request.salience_signals = {{"novelty", 0.8}};
        
        auto result = adapter.select_candidate_result(request);
        assert(result.valid());
        assert(result.success);
        assert(result.value);
        assert(result.value->workspace_id == "ws1");
        assert(result.value->session_id == "session1");
        assert(result.value->created_at == request.observed_at);
        assert(result.value->capacity == WorkspaceConfig{}.capacity);
        assert(result.value->valid());
        assert(result.value->active_candidate_ids ==
               std::vector<std::string>{"candidate-1"});

        auto invalid = request;
        invalid.source_refs.clear();
        assert(!adapter.select_candidate_result(invalid).success);

        CanonicalEvent legacy;
        legacy.event_id = "legacy";
        assert(!adapter.select_result(legacy).success);
        
        std::cout << "GlobalWorkspaceAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
