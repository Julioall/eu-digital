#include "core/adapters/metacognition_curiosity_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto engine = std::make_shared<MetacognitionCuriosityEngine>();
        MetacognitionCuriosityAdapter adapter(engine);
        
        contracts::WorkspaceSnapshot ws_snap;
        ws_snap.workspace_id = "ws1";
        
        auto result = adapter.evaluate_result(ws_snap);
        assert(result.valid());
        assert(result.success);
        assert(result.value);
        assert(result.value->valid());
        
        std::cout << "MetacognitionCuriosityAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
