#include "core/cognitive_coordinator.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace eu_digital;

namespace {

contracts::CognitiveCycleInputV1 input(std::size_t index) {
    contracts::CognitiveCycleInputV1 value;
    value.correlation_id = "benchmark-" + std::to_string(index);
    value.event_id = value.correlation_id;
    value.source = "benchmark";
    value.event_type = "benchmark.observation";
    value.session_id = "benchmark-session";
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.epoch_seconds = 1.0;
    value.modality = "synthetic";
    return value;
}

}  // namespace

int main() {
    constexpr std::size_t samples = 4'000;
    CapabilityRegistry registry;
    CognitiveCoordinatorConfig config;
    config.max_queue_size = samples;
    config.auto_start = false;
    CognitiveCoordinator coordinator(registry, config);

    std::vector<std::uint64_t> durations;
    durations.reserve(samples);
    for (std::size_t index = 0; index < samples; ++index) {
        const auto started = std::chrono::steady_clock::now();
        const auto receipt = coordinator.enqueue_input(input(index));
        const auto finished = std::chrono::steady_clock::now();
        if (receipt.status != EnqueueStatusV1::accepted) return 2;
        durations.push_back(static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(finished - started)
                .count()));
    }
    std::sort(durations.begin(), durations.end());
    const auto median_ns = durations[durations.size() / 2];
    const auto p95_ns = durations[durations.size() * 95 / 100];
    std::cout << "{\"samples\":" << samples << ",\"median_ns\":" << median_ns
              << ",\"p95_ns\":" << p95_ns << ",\"limit_ns\":1000000}\n";

    coordinator.start();
    coordinator.wait_idle();
    coordinator.stop();
    return median_ns <= 1'000'000 ? 0 : 1;
}
