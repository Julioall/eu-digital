#include "core/adapters/cognitive_port_factory.hpp"
#include "core/capability_runtime.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace eu_digital;

namespace {

class MockPatternLearningPort final : public IPatternLearningPort {
public:
    contracts::PatternLearningResult observe(
        const contracts::PatternLearningObservation&) override {
        contracts::ObservedPattern pattern;
        pattern.pattern_id = "mock-pattern";
        return contracts::PatternLearningResult::ok(std::move(pattern));
    }

    contracts::PatternLearningResult feedback(
        const contracts::PatternLearningFeedback&) override {
        return contracts::PatternLearningResult::failed(
            "pattern_learning.feedback", "not_supported", "fixture");
    }

    std::vector<contracts::ObservedPattern> snapshot() const override {
        return {};
    }
};

CapabilityDescriptor pattern_descriptor(std::string implementation_id) {
    CapabilityDescriptor descriptor;
    descriptor.capability_id = "cognition.pattern_learning";
    descriptor.implementation_id = std::move(implementation_id);
    descriptor.implementation_version = "1.0.0";
    descriptor.kind = "cognitive_service";
    descriptor.provides.push_back({"learn.patterns", "urn:eu-digital:pattern:1"});
    descriptor.supports_hot_plug = true;
    return descriptor;
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
    return std::find(values.begin(), values.end(), value) != values.end();
}

}  // namespace

void test_cognitive_port_factory() {
    std::cout << "Starting test_cognitive_port_factory" << std::endl;
    
    WorldModelConfig config;
    auto wm = std::make_shared<WorldModel>(config, "test_stream");
    auto store = std::make_shared<EpisodicMemoryStore>();
    auto learner = std::make_shared<PatternLearner>(PatternConfig{}, "factory-stream");
    
    auto prediction_port = CognitivePortFactory::create_prediction_port(wm);
    auto memory_write_port = CognitivePortFactory::create_memory_write_port(store);
    auto memory_retrieval_port = CognitivePortFactory::create_memory_retrieval_port(store);
    auto boundary_port = CognitivePortFactory::create_episode_boundary_port();
    auto pattern_port = CognitivePortFactory::create_pattern_learning_port(learner);
    
    if (!prediction_port || !memory_write_port || !memory_retrieval_port ||
        !boundary_port || !pattern_port) {
        throw std::runtime_error("Factory returned null port");
    }
}

void test_registry_absence_substitution_removal_and_reinstallation() {
    CapabilityRegistry registry;
    assert(!registry.resolve<IPatternLearningPort>("learn.patterns"));

    auto learner = std::make_shared<PatternLearner>(PatternConfig{}, "registry-stream");
    auto real = CognitivePortFactory::create_pattern_learning_port(learner);
    const auto real_descriptor = pattern_descriptor("native.pattern_learning.port");
    registry.register_instance(real_descriptor, real, 10);

    assert(registry.resolve<IPatternLearningPort>("learn.patterns") == real);
    assert(contains(registry.self_model().available, real_descriptor.implementation_id));

    auto mock = std::make_shared<MockPatternLearningPort>();
    const auto mock_descriptor = pattern_descriptor("fixture.pattern_learning.port");
    registry.register_instance(mock_descriptor, mock, 20);
    assert(registry.resolve<IPatternLearningPort>("learn.patterns") == mock);

    registry.transition(mock_descriptor.implementation_id, CapabilityState::removed, "test_removal");
    assert(registry.resolve<IPatternLearningPort>("learn.patterns") == real);
    assert(contains(registry.self_model().removed, mock_descriptor.implementation_id));

    registry.transition(real_descriptor.implementation_id, CapabilityState::removed, "test_absence");
    assert(!registry.resolve<IPatternLearningPort>("learn.patterns"));

    registry.register_instance(real_descriptor, real, 10);
    assert(registry.resolve<IPatternLearningPort>("learn.patterns") == real);
    assert(contains(registry.self_model().available, real_descriptor.implementation_id));
}

int main() {
    try {
        test_cognitive_port_factory();
        test_registry_absence_substitution_removal_and_reinstallation();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}
