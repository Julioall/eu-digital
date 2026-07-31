#include "core/adapters/cognitive_port_factory.hpp"

#include <iostream>
#include <stdexcept>
#include <memory>

using namespace eu_digital;

void test_cognitive_port_factory() {
    std::cout << "Starting test_cognitive_port_factory" << std::endl;
    
    WorldModelConfig config;
    auto wm = std::make_shared<WorldModel>(config, "test_stream");
    auto store = std::make_shared<EpisodicMemoryStore>();
    
    auto prediction_port = CognitivePortFactory::create_prediction_port(wm);
    auto memory_write_port = CognitivePortFactory::create_memory_write_port(store);
    auto memory_retrieval_port = CognitivePortFactory::create_memory_retrieval_port(store);
    auto boundary_port = CognitivePortFactory::create_episode_boundary_port();
    
    if (!prediction_port || !memory_write_port || !memory_retrieval_port || !boundary_port) {
        throw std::runtime_error("Factory returned null port");
    }
}

int main() {
    try {
        test_cognitive_port_factory();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
