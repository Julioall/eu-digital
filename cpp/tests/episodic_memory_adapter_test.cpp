#include "core/adapters/episodic_memory_adapter.hpp"
#include "core/episodic_memory.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace eu_digital;

void test_episodic_memory_adapter() {
    std::cout << "Starting test_episodic_memory_adapter" << std::endl;
    auto store = std::make_shared<EpisodicMemoryStore>();
    EpisodicMemoryAdapter adapter(store);
    
    CanonicalEvent ev;
    ev.event_id = "test-event";
    auto write_res = adapter.store_event(ev);
    
    if (!write_res.success) {
        throw std::runtime_error("Write failed");
    }
    
    auto retrieve_res = adapter.retrieve("test-query", 5);
    // Deve retornar 0 itens pois na simulação não preenchemos o store de verdade.
    if (!retrieve_res.items.empty()) {
        throw std::runtime_error("Expected empty retrieve");
    }
}

int main() {
    try {
        test_episodic_memory_adapter();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
