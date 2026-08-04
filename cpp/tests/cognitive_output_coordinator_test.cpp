#include "core/cognitive_output_coordinator.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace eu_digital;

namespace {

contracts::CognitiveOutputRequestV1 request(
    std::string id,
    contracts::CognitiveOutputIntentV1 intent =
        contracts::CognitiveOutputIntentV1::requested_response) {
    contracts::CognitiveOutputRequestV1 value;
    value.request_id = std::move(id);
    value.correlation_id = "correlation-1";
    value.input_event_id = "event-1";
    value.intent = intent;
    value.occurred_at = "2026-08-04T12:00:00Z";
    value.critical = intent == contracts::CognitiveOutputIntentV1::question ||
                     intent ==
                         contracts::CognitiveOutputIntentV1::requested_response;
    value.reason = "fixture";
    value.input_content = {{"text", "Olá"}};
    value.evidence_refs = {"event-1"};
    assert(value.valid());
    return value;
}

CapabilityDescriptor descriptor(std::string operation,
                                std::string implementation,
                                std::string kind) {
    CapabilityDescriptor value;
    value.capability_id = operation;
    value.implementation_id = std::move(implementation);
    value.implementation_version = "1.0.0";
    value.kind = std::move(kind);
    value.supports_hot_plug = true;
    value.provides.push_back({std::move(operation), "1.0"});
    return value;
}

class Renderer final : public ILanguageRenderer {
public:
    explicit Renderer(std::string renderer_id, bool invalid = false,
                      bool throws = false)
        : renderer_id_(std::move(renderer_id)), invalid_(invalid),
          throws_(throws) {}

    contracts::ValidatedDialogueOutputV1 render(
        const contracts::CognitiveOutputRequestV1& value) override {
        ++calls;
        if (throws_) throw std::runtime_error("renderer failed");
        contracts::LanguageRenderingCandidateV1 candidate;
        candidate.request_id = value.request_id;
        candidate.intent = value.intent;
        candidate.rendered_text = renderer_id_;
        candidate.evidence_refs = {"event-1"};
        auto output = contracts::ValidatedDialogueOutputV1::from_candidate(
            value, candidate, renderer_id_);
        if (invalid_) output.request_id = "wrong-request";
        return output;
    }

    int calls{0};

private:
    std::string renderer_id_;
    bool invalid_{false};
    bool throws_{false};
};

class Presenter final : public IPresentationPort {
public:
    explicit Presenter(bool fail = false) : fail_(fail) {}

    contracts::PortResult<bool> present(
        const contracts::ValidatedDialogueOutputV1& output) override {
        outputs.push_back(output);
        if (fail_) {
            return contracts::PortResult<bool>::failed(
                kPresentationOperation, "surface_failed", "fixture failure");
        }
        return contracts::PortResult<bool>::ok(true);
    }

    std::vector<contracts::ValidatedDialogueOutputV1> outputs;

private:
    bool fail_{false};
};

class GateRenderer final : public ILanguageRenderer {
public:
    contracts::ValidatedDialogueOutputV1 render(
        const contracts::CognitiveOutputRequestV1& value) override {
        {
            std::lock_guard lock(mutex_);
            started_ = true;
        }
        ready_.notify_all();
        {
            std::unique_lock lock(mutex_);
            ready_.wait(lock, [&] { return released_; });
        }
        contracts::LanguageRenderingCandidateV1 candidate;
        candidate.request_id = value.request_id;
        candidate.intent = value.intent;
        candidate.rendered_text = "released";
        return contracts::ValidatedDialogueOutputV1::from_candidate(
            value, candidate, "gate-renderer");
    }

    void wait_started() {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [&] { return started_; });
    }

    void release() {
        {
            std::lock_guard lock(mutex_);
            released_ = true;
        }
        ready_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable ready_;
    bool started_{false};
    bool released_{false};
};

}  // namespace

int main() {
    {
        CapabilityRegistry absent;
        CognitiveOutputCoordinator coordinator(absent);
        assert(coordinator.enqueue(request("absent")).accepted);
        coordinator.wait_idle();
        assert(coordinator.metrics().renderer_unavailable == 1);
    }

    CapabilityRegistry registry;
    auto low = std::make_shared<Renderer>("low-renderer");
    auto surface = std::make_shared<Presenter>();
    const auto low_descriptor =
        descriptor(kLanguageRenderOperation, "renderer-low", "language");
    const auto surface_descriptor =
        descriptor(kPresentationOperation, "surface-one", "presentation");
    registry.register_instance<ILanguageRenderer>(low_descriptor, low, 10);
    registry.register_instance<IPresentationPort>(surface_descriptor, surface, 10);

    CognitiveOutputCoordinator coordinator(registry);
    const auto first = coordinator.enqueue(request("first"));
    assert(first.accepted);
    assert(first.duration_microseconds < 1000);
    coordinator.wait_idle();
    assert(surface->outputs.size() == 1);
    assert(surface->outputs.back().rendered_text == "low-renderer");
    assert(coordinator.metrics().presented == 1);

    assert(!coordinator.enqueue(request("first")).accepted);
    assert(coordinator.metrics().duplicates == 1);

    registry.transition("renderer-low", CapabilityState::removed, "removed");
    assert(coordinator.enqueue(request("removed")).accepted);
    coordinator.wait_idle();
    assert(coordinator.metrics().renderer_unavailable == 1);

    registry.register_instance<ILanguageRenderer>(low_descriptor, low, 10);
    assert(coordinator.enqueue(request("reinstalled")).accepted);
    coordinator.wait_idle();
    assert(surface->outputs.size() == 2);

    auto high = std::make_shared<Renderer>("high-renderer");
    registry.register_instance<ILanguageRenderer>(
        descriptor(kLanguageRenderOperation, "renderer-high", "language"),
        high, 20);
    assert(coordinator.enqueue(request("substituted")).accepted);
    coordinator.wait_idle();
    assert(surface->outputs.back().rendered_text == "high-renderer");

    registry.transition("surface-one", CapabilityState::removed, "removed");
    assert(coordinator.enqueue(request("no-surface")).accepted);
    coordinator.wait_idle();
    assert(coordinator.metrics().presentation_unavailable == 1);
    registry.register_instance<IPresentationPort>(surface_descriptor, surface, 10);

    auto failing_surface = std::make_shared<Presenter>(true);
    registry.register_instance<IPresentationPort>(
        descriptor(kPresentationOperation, "surface-failing", "presentation"),
        failing_surface, 20);
    assert(coordinator.enqueue(request("surface-failure")).accepted);
    coordinator.wait_idle();
    assert(coordinator.metrics().presentation_failures == 1);
    registry.transition("surface-failing", CapabilityState::removed, "removed");

    std::vector<std::uint64_t> latencies;
    for (int index = 0; index < 100; ++index) {
        const auto result = coordinator.enqueue(
            request("latency-" + std::to_string(index)));
        latencies.push_back(result.duration_microseconds);
    }
    coordinator.wait_idle();
    std::sort(latencies.begin(), latencies.end());
    assert(latencies[latencies.size() / 2] < 1000);
    std::cout << "cognitive_output_enqueue_median_us="
              << latencies[latencies.size() / 2] << '\n';

    {
        CapabilityRegistry invalid_registry;
        auto invalid = std::make_shared<Renderer>("invalid", true);
        auto invalid_surface = std::make_shared<Presenter>();
        invalid_registry.register_instance<ILanguageRenderer>(
            kLanguageRenderOperation, invalid);
        invalid_registry.register_instance<IPresentationPort>(
            kPresentationOperation, invalid_surface);
        CognitiveOutputCoordinator invalid_coordinator(invalid_registry);
        assert(invalid_coordinator.enqueue(request("invalid")).accepted);
        invalid_coordinator.wait_idle();
        assert(invalid_coordinator.metrics().renderer_failures == 1);
        assert(invalid_surface->outputs.empty());
    }

    {
        CapabilityRegistry queue_registry;
        auto gate = std::make_shared<GateRenderer>();
        auto queue_surface = std::make_shared<Presenter>();
        queue_registry.register_instance<ILanguageRenderer>(
            kLanguageRenderOperation, gate);
        queue_registry.register_instance<IPresentationPort>(
            kPresentationOperation, queue_surface);
        CognitiveOutputCoordinatorConfig config;
        config.max_queue_size = 1;
        CognitiveOutputCoordinator bounded(queue_registry, config);
        assert(bounded.enqueue(request("gate-1")).accepted);
        gate->wait_started();
        assert(bounded.enqueue(request("gate-2")).accepted);
        const auto dropped = bounded.enqueue(request("gate-3"));
        assert(!dropped.accepted);
        assert(dropped.reason_code == "discarded_backpressure");
        gate->release();
        bounded.wait_idle();
        assert(bounded.metrics().backpressure == 1);
        assert(queue_surface->outputs.size() == 2);
    }
}
