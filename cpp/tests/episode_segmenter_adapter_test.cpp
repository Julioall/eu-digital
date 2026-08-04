#include "core/adapters/episode_segmenter_adapter.hpp"

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
