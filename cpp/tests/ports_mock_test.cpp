#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/iprediction_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/iself_model_query_port.hpp"

#include <iostream>
#include <stdexcept>

using namespace eu_digital;

class MockMemoryWritePort : public IMemoryWritePort {
public:
    MemoryWriteResult store_event(const CanonicalEvent& event) override {
        return MemoryWriteResult::ok("mock-id");
    }
};

void test_memory_write_port() {
    std::cout << "Starting test_memory_write_port" << std::endl;
    MockMemoryWritePort port;
    CanonicalEvent ev;
    auto result = port.store_event(ev);
    if (!result.success || result.memory_id != "mock-id") {
        throw std::runtime_error("Port mock failed");
    }
}

int main() {
    try {
        test_memory_write_port();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
