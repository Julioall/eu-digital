#include "core/adapters/global_workspace_adapter.hpp"
#include <cassert>
#include <iostream>

using namespace eu_digital;

int main() {
    try {
        auto ws = std::make_shared<GlobalWorkspace>("ws1", "session1", WorkspaceConfig{});
        GlobalWorkspaceAdapter adapter(ws);
        
        CanonicalEvent ev;
        ev.event_id = "ev1";
        ev.monotonic_ns = 12345;
        
        auto snapshot = adapter.select(ev);
        assert(!snapshot.workspace_id.empty());
        
        std::cout << "GlobalWorkspaceAdapter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
