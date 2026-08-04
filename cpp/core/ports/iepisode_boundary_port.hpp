#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/cognitive_cycle_v1.hpp"
#include "core/contracts/episode_update.hpp"
#include "core/contracts/port_result.hpp"

#include <stdexcept>

namespace eu_digital {

class IEpisodeBoundaryPort {
public:
    virtual ~IEpisodeBoundaryPort() = default;

    virtual EpisodeUpdate evaluate(const CanonicalEvent& event) = 0;

    virtual EpisodeUpdate evaluate_observation(
        const contracts::EpisodeObservationRequest&) {
        throw std::logic_error("episode observation requests are not implemented");
    }

    contracts::PortResult<EpisodeUpdate> evaluate_result(const CanonicalEvent& event) {
        return contracts::capture_port_result<EpisodeUpdate>(
            "episode_boundary.evaluate", [&] { return evaluate(event); });
    }

    contracts::PortResult<EpisodeUpdate> evaluate_observation_result(
        const contracts::EpisodeObservationRequest& request) {
        return contracts::capture_port_result<EpisodeUpdate>(
            "episode_boundary.evaluate_observation",
            [&] { return evaluate_observation(request); });
    }

    virtual contracts::PortResult<contracts::EpisodeSegmentationResponseV1>
    segment_observation(
        const contracts::EpisodeObservationRequest& request,
        const contracts::PortInvocationContextV1& context) {
        if (context.stop_requested()) {
            return contracts::PortResult<contracts::EpisodeSegmentationResponseV1>::failed(
                "episode_boundary.segment_observation", "cancelled",
                "cycle invocation was cancelled");
        }
        const auto result = evaluate_observation_result(request);
        if (!result.success || !result.value) {
            return contracts::PortResult<contracts::EpisodeSegmentationResponseV1>::failed(
                "episode_boundary.segment_observation",
                result.error ? result.error->code : "adapter_delegation_error",
                result.error ? result.error->message : "episode update unavailable");
        }
        contracts::EpisodeSegmentationResponseV1 response;
        response.update = *result.value;
        return contracts::PortResult<contracts::EpisodeSegmentationResponseV1>::ok(
            std::move(response));
    }
};

} // namespace eu_digital
