#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/memory_write_result.hpp"
#include "core/contracts/port_result.hpp"

#include <stdexcept>

namespace eu_digital {

class IMemoryWritePort {
public:
    virtual ~IMemoryWritePort() = default;

    virtual MemoryWriteResult store_event(const CanonicalEvent& event) = 0;

    virtual MemoryWriteResult store_episode(
        const contracts::EpisodeWriteRequest&) {
        throw std::logic_error("episode write requests are not implemented");
    }

    contracts::PortResult<MemoryWriteResult> store_event_result(
        const CanonicalEvent& event) {
        return contracts::capture_port_result<MemoryWriteResult>(
            "memory.store_event", [&] { return store_event(event); });
    }

    contracts::PortResult<MemoryWriteResult> store_episode_result(
        const contracts::EpisodeWriteRequest& request) {
        return contracts::capture_port_result<MemoryWriteResult>(
            "memory.store_episode", [&] { return store_episode(request); });
    }
};

} // namespace eu_digital
