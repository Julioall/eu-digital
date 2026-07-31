#include "core/adapters/functional_self_model_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto sm = std::make_shared<VersionedFunctionalSelfModel>("sm1", "2026-07-31T12:00:00Z");
        FunctionalSelfModelAdapter adapter(sm);
        
        auto snapshot = adapter.query_constraints();
        assert(snapshot.valid());
        assert(snapshot.model_id == "sm1");
        
        std::cout << "FunctionalSelfModelAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
