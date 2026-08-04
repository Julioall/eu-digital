#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/iprediction_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/imemory_retrieval_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/iself_model_query_port.hpp"
#include "core/ports/ipattern_learning_port.hpp"

#include <iostream>
#include <stdexcept>

using namespace eu_digital;

class MockMemoryWritePort : public IMemoryWritePort {
public:
    MemoryWriteResult store_event(const CanonicalEvent& event) override {
        return MemoryWriteResult::ok("mock-id");
    }
};

class MockPatternLearningPort final : public IPatternLearningPort {
public:
    contracts::PatternLearningResult observe(
        const contracts::PatternLearningObservation& observation) override {
        contracts::ObservedPattern pattern;
        pattern.pattern_id = "mock-pattern";
        pattern.observation_refs = {observation.observation_ref};
        return contracts::PatternLearningResult::ok(std::move(pattern));
    }

    contracts::PatternLearningResult feedback(
        const contracts::PatternLearningFeedback&) override {
        return contracts::PatternLearningResult::failed(
            "pattern_learning.feedback", "unsupported_feedback",
            "mock does not retain patterns");
    }

    std::vector<contracts::ObservedPattern> snapshot() const override {
        return {};
    }
};

void test_memory_write_port() {
    std::cout << "Starting test_memory_write_port" << std::endl;
    MockMemoryWritePort port;
    CanonicalEvent ev;
    auto result = port.store_event_result(ev);
    if (!result.valid() || !result.success || !result.value ||
        !result.value->success || result.value->memory_id != "mock-id") {
        throw std::runtime_error("Port mock failed");
    }
}

void test_pattern_learning_port() {
    MockPatternLearningPort port;
    contracts::PatternLearningObservation observation;
    observation.features = {{"application.vscode", 1.0}};
    observation.observation_ref = "event-1";
    observation.occurred_epoch = 1.0;

    const auto result = port.observe(observation);
    if (!result.success || !result.value ||
        result.value->observation_refs != std::vector<std::string>{"event-1"}) {
        throw std::runtime_error("Pattern port mock failed");
    }

    const auto snapshot = port.snapshot_result();
    if (!snapshot.valid() || !snapshot.success || !snapshot.value) {
        throw std::runtime_error("Pattern snapshot result failed");
    }
}

int main() {
    try {
        test_memory_write_port();
        test_pattern_learning_port();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
