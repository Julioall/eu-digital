#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/memory_write_result.hpp"
#include "core/contracts/port_result.hpp"

namespace eu_digital {

class IMemoryWritePort {
public:
    virtual ~IMemoryWritePort() = default;

    virtual MemoryWriteResult store_event(const CanonicalEvent& event) = 0;

    contracts::PortResult<MemoryWriteResult> store_event_result(
        const CanonicalEvent& event) {
        return contracts::capture_port_result<MemoryWriteResult>(
            "memory.store_event", [&] { return store_event(event); });
    }
};

} // namespace eu_digital
