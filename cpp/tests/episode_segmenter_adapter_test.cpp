#include "core/adapters/episode_segmenter_adapter.hpp"

#include <iostream>
#include <stdexcept>

using namespace eu_digital;

void test_episode_segmenter_adapter() {
    std::cout << "Starting test_episode_segmenter_adapter" << std::endl;
    EpisodeSegmenterAdapter adapter;
    
    CanonicalEvent ev;
    ev.event_id = "test-event-1";
    ev.monotonic_ns = 1000000000; // 1 segundo
    
    auto update = adapter.evaluate(ev);
    
    // O primeiro evento cria uma boundary (start)
    if (!update.is_new_episode) {
        throw std::runtime_error("Expected new episode on first event");
    }
}

int main() {
    try {
        test_episode_segmenter_adapter();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
