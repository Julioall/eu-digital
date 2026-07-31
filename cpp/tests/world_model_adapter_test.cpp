#include "core/adapters/world_model_adapter.hpp"
#include "core/world_model.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace eu_digital;

void test_world_model_adapter() {
    std::cout << "Starting test_world_model_adapter" << std::endl;
    WorldModelConfig config;
    auto wm = std::make_shared<WorldModel>(config, "test_stream");
    
    WorldModelAdapter adapter(wm);
    
    // Predição vazia deve falhar de forma gracefully retornando inválido
    auto failed = adapter.predict({}, "now");
    if (failed.valid()) {
        throw std::runtime_error("Expected invalid prediction on empty context");
    }
    
    wm->observe("state1", "ref1", 10.0);
    wm->observe("state2", "ref2", 20.0);
    
    auto success = adapter.predict({"state1"}, "now");
    if (!success.valid() || success.predicted_distribution.empty()) {
        throw std::runtime_error("Expected valid prediction");
    }
}

int main() {
    try {
        test_world_model_adapter();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
