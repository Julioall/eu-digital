#include "core/adapters/cognitive_decision_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto so = std::make_shared<SuggestionOrchestrator>();
        CognitiveDecisionAdapter adapter(so);
        
        contracts::DecisionRequest request;
        request.event_id = "event-1";
        request.event_type = "user_explicit_question";
        request.occurred_at = "2026-08-04T12:00:00Z";
        request.hypothesis_id = "hypothesis-1";
        request.confidence = 0.7;
        request.information_gain = 0.4;
        request.evidence_ids = {"event-1"};
        request.reason = "explicit user request";
        request.workspace_snapshot_id = "snapshot-1";
        auto result = adapter.decide_evidence_result(request);
        assert(result.valid());
        assert(result.success);
        assert(result.value);
        assert(result.value->success);
        assert(result.value->intent == "requested_response");
        assert(result.value->reason == "explicit_user_request");
        assert(!result.value->target_action.empty());
        assert(so->decisions().empty());
        const auto metrics_before = so->metrics_json();
        const auto repeated = adapter.decide_evidence_result(request);
        assert(repeated.success);
        assert(so->decisions().empty());
        assert(so->metrics_json() == metrics_before);

        auto proactive = request;
        proactive.event_id = "event-2";
        proactive.event_type = "other_observation";
        proactive.occurred_at = "2026-08-04T12:10:00Z";
        proactive.hypothesis_id = "hypothesis-2";
        proactive.evidence_ids = {"event-2"};
        proactive.workspace_snapshot_id.reset();
        const auto proactive_result = adapter.decide_evidence_result(proactive);
        assert(proactive_result.success);
        assert(so->decisions().size() == 1);

        auto invalid = request;
        invalid.occurred_at.clear();
        assert(!adapter.decide_evidence_result(invalid).success);

        CanonicalEvent legacy_event;
        legacy_event.event_id = "legacy";
        CognitiveCycleContext legacy_context;
        assert(!adapter.decide_result(legacy_event, legacy_context).success);
        
        std::cout << "CognitiveDecisionAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
