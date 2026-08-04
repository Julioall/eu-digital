#include "core/adapters/cognitive_decision_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto so = std::make_shared<SuggestionOrchestrator>();
        CognitiveDecisionAdapter adapter(so);
        
        CanonicalEvent ev;
        ev.event_id = "ev1";
        ev.monotonic_ns = 1000;
        
        CognitiveCycleContext ctx;
        auto result = adapter.decide_result(ev, ctx);
        assert(result.valid());
        assert(result.success);
        assert(result.value);
        assert(result.value->success);
        
        std::cout << "CognitiveDecisionAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
