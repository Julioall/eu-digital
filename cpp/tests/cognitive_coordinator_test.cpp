#include "core/cognitive_coordinator.hpp"
#include "core/event_bus.hpp"
#include "core/capability_runtime.hpp"
#include "core/ports/iepisode_boundary_port.hpp"
#include "core/ports/imemory_write_port.hpp"
#include "core/ports/iprediction_port.hpp"
#include "core/ports/iworkspace_selection_port.hpp"
#include "core/ports/imetacognition_port.hpp"
#include "core/ports/iself_model_query_port.hpp"
#include "core/ports/icognitive_decision_port.hpp"

#include <cassert>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <memory>
#include <vector>
#include <iostream>

using namespace eu_digital;

class MockEpisodeBoundaryPort : public IEpisodeBoundaryPort {
public:
    EpisodeUpdate evaluate(const CanonicalEvent& event) override {
        return EpisodeUpdate{};
    }
};

class MockMemoryWritePort : public IMemoryWritePort {
public:
    MemoryWriteResult store_event(const CanonicalEvent& event) override {
        return MemoryWriteResult{};
    }
};

class MockPredictionPort : public IPredictionPort {
public:
    PredictionAssessment predict(
        const std::vector<std::string>& context,
        const std::string& predicted_at,
        const std::vector<std::string>& candidate_states = {}) override {
        return PredictionAssessment{};
    }
    
    PredictionAssessment score(
        const PredictionAssessment& prediction,
        const std::string& observed_state,
        const std::string& observed_at) override {
        return PredictionAssessment{};
    }
};

class MockWorkspaceSelectionPort : public IWorkspaceSelectionPort {
public:
    contracts::WorkspaceSnapshot select(const CanonicalEvent& event) override {
        return contracts::WorkspaceSnapshot{};
    }
};

class MockMetacognitionPort : public IMetacognitionPort {
public:
    contracts::MetacognitiveAssessment evaluate(const contracts::WorkspaceSnapshot& workspace) override {
        return contracts::MetacognitiveAssessment{};
    }
};

class MockSelfModelQueryPort : public ISelfModelQueryPort {
public:
    SelfConstraintSnapshot query_constraints() override {
        return SelfConstraintSnapshot{};
    }
};

class MockCognitiveDecisionPort : public ICognitiveDecisionPort {
public:
    CognitiveDecision decide(const CanonicalEvent& event, const CognitiveCycleContext& ctx) override {
        return CognitiveDecision{};
    }
};

class FailingDecisionPort : public ICognitiveDecisionPort {
public:
    CognitiveDecision decide(const CanonicalEvent& event, const CognitiveCycleContext& ctx) override {
        throw std::runtime_error("Simulated decision failure");
    }
};

void test_happy_path() {
    CapabilityRegistry registry;

    registry.register_instance<IEpisodeBoundaryPort>("episode_boundary", std::make_shared<MockEpisodeBoundaryPort>());
    registry.register_instance<IMemoryWritePort>("memory_write", std::make_shared<MockMemoryWritePort>());
    registry.register_instance<IPredictionPort>("prediction", std::make_shared<MockPredictionPort>());
    registry.register_instance<IWorkspaceSelectionPort>("workspace", std::make_shared<MockWorkspaceSelectionPort>());
    registry.register_instance<IMetacognitionPort>("metacognition", std::make_shared<MockMetacognitionPort>());
    registry.register_instance<ISelfModelQueryPort>("self_model", std::make_shared<MockSelfModelQueryPort>());
    registry.register_instance<ICognitiveDecisionPort>("decision", std::make_shared<MockCognitiveDecisionPort>());

    CognitiveCoordinator coordinator(registry);

    CanonicalEvent event;
    event.event_id = "test-event-123";
    event.payload = "hello";

    coordinator.enqueue(event);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    coordinator.stop();

    auto logs = coordinator.get_logs();
    
    assert(logs.size() == 3);
    assert(logs[0].event_id == "test-event-123");
    assert(logs[0].state == CycleState::queued);
    
    assert(logs[1].event_id == "test-event-123");
    assert(logs[1].state == CycleState::processing);
    
    assert(logs[2].event_id == "test-event-123");
    assert(logs[2].state == CycleState::completed);
}

void test_missing_ports_yields_degraded() {
    CapabilityRegistry registry;

    CognitiveCoordinator coordinator(registry);

    CanonicalEvent event;
    event.event_id = "test-missing-ports";

    coordinator.enqueue(event);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    coordinator.stop();

    auto logs = coordinator.get_logs();
    
    assert(logs.size() == 3);
    assert(logs[2].state == CycleState::degraded);
    assert(logs[2].reason.find("missing_episode_port") != std::string::npos);
    assert(logs[2].reason.find("missing_decision_port") != std::string::npos);
}

void test_exception_in_port_yields_degraded() {
    CapabilityRegistry registry;

    registry.register_instance<IEpisodeBoundaryPort>("episode_boundary", std::make_shared<MockEpisodeBoundaryPort>());
    registry.register_instance<IMemoryWritePort>("memory_write", std::make_shared<MockMemoryWritePort>());
    registry.register_instance<IPredictionPort>("prediction", std::make_shared<MockPredictionPort>());
    registry.register_instance<IWorkspaceSelectionPort>("workspace", std::make_shared<MockWorkspaceSelectionPort>());
    registry.register_instance<IMetacognitionPort>("metacognition", std::make_shared<MockMetacognitionPort>());
    registry.register_instance<ISelfModelQueryPort>("self_model", std::make_shared<MockSelfModelQueryPort>());
    
    // Inject failing port
    registry.register_instance<ICognitiveDecisionPort>("decision", std::make_shared<FailingDecisionPort>());

    CognitiveCoordinator coordinator(registry);
    CanonicalEvent event;
    event.event_id = "test-fail-port";

    coordinator.enqueue(event);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    coordinator.stop();

    auto logs = coordinator.get_logs();
    
    assert(logs.size() == 3);
    assert(logs[2].state == CycleState::degraded);
    assert(logs[2].reason.find("decision_error: Simulated decision failure") != std::string::npos);
}

void test_backpressure_drops_events() {
    CapabilityRegistry registry;

    // Set max queue size to 2
    CognitiveCoordinator coordinator(registry, 2);

    for (int i = 0; i < 5; ++i) {
        CanonicalEvent event;
        event.event_id = "event-" + std::to_string(i);
        coordinator.enqueue(event);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    coordinator.stop();

    auto logs = coordinator.get_logs();
    
    bool found_discarded = false;
    for (const auto& log : logs) {
        if (log.state == CycleState::discarded) {
            found_discarded = true;
            break;
        }
    }
    
    assert(found_discarded);
}

int main() {
    test_happy_path();
    test_missing_ports_yields_degraded();
    test_exception_in_port_yields_degraded();
    test_backpressure_drops_events();
    
    std::cout << "cognitive coordinator tests passed\n";
    return 0;
}
