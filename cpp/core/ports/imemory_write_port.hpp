#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/memory_write_result.hpp"

namespace eu_digital {

class IMemoryWritePort {
public:
    virtual ~IMemoryWritePort() = default;

    virtual MemoryWriteResult store_event(const CanonicalEvent& event) = 0;
};

} // namespace eu_digital
