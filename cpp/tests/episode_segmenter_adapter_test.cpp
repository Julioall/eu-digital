#include "core/adapters/episode_segmenter_adapter.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace eu_digital;

void test_episode_segmenter_adapter() {
    std::cout << "Starting test_episode_segmenter_adapter" << std::endl;
    EpisodeSegmenterAdapter adapter;
    
    contracts::EpisodeObservationRequest observation;
    observation.event_id = "test-event-1";
    observation.session_id = "session-1";
    observation.occurred_at = "2026-08-04T12:00:00Z";
    observation.epoch_seconds = 1.0;
    observation.application = "editor";
    observation.modality = "system_activity";
    
    auto result = adapter.evaluate_observation_result(observation);
    if (!result.valid() || !result.success || !result.value) {
        throw std::runtime_error("Expected successful structured result");
    }
    const auto& update = *result.value;
    
    // O primeiro evento cria uma boundary (start)
    if (!update.is_new_episode) {
        throw std::runtime_error("Expected new episode on first event");
    }
    if (update.episode_id.empty()) {
        throw std::runtime_error("Expected mapped episode id");
    }

    auto invalid = observation;
    invalid.session_id.clear();
    if (adapter.evaluate_observation_result(invalid).success) {
        throw std::runtime_error("Expected invalid observation rejection");
    }

    CanonicalEvent legacy;
    legacy.event_id = "legacy";
    const auto rejected = adapter.evaluate_result(legacy);
    if (rejected.success || !rejected.error) {
        throw std::runtime_error("Expected legacy request rejection");
    }

    contracts::PortInvocationContextV1 context;
    context.correlation_id = "checkpoint-test";
    context.deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(1);
    const auto captured = adapter.capture_state(context);
    if (!captured.success || !captured.value || !captured.value->valid()) {
        throw std::runtime_error("Expected a valid episode checkpoint");
    }

    EpisodeSegmenterAdapter restored;
    const auto restore_result = restored.restore_state(*captured.value, context);
    if (!restore_result.success || !restore_result.value ||
        restore_result.value->provider_id != restored.provider_id()) {
        throw std::runtime_error("Expected episode checkpoint restoration");
    }

    observation.event_id = "test-event-2";
    observation.occurred_at = "2026-08-04T12:00:01Z";
    observation.epoch_seconds = 2.0;
    const auto continued = restored.evaluate_observation_result(observation);
    if (!continued.success || !continued.value || continued.value->is_new_episode) {
        throw std::runtime_error("Restored episode must continue at exact boundary");
    }

    const auto before_invalid = restored.capture_state(context);
    auto invalid_fragment = *captured.value;
    invalid_fragment.entries["session.0.event.0.epoch_seconds"] = "not-a-number";
    if (restored.restore_state(invalid_fragment, context).success) {
        throw std::runtime_error("Invalid episode state must be rejected");
    }
    const auto after_invalid = restored.capture_state(context);
    if (!before_invalid.value || !after_invalid.value ||
        before_invalid.value->to_json() != after_invalid.value->to_json()) {
        throw std::runtime_error("Failed restore must leave episode state unchanged");
    }

    auto wrong_provider = *captured.value;
    wrong_provider.provider_id = "replacement-provider";
    if (restored.restore_state(wrong_provider, context).success) {
        throw std::runtime_error("State from a substituted provider must be rejected");
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
