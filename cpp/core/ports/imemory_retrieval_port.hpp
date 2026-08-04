#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/memory_retrieval_result.hpp"
#include "core/contracts/port_result.hpp"
#include <string>

namespace eu_digital {

class IMemoryRetrievalPort {
public:
    virtual ~IMemoryRetrievalPort() = default;

    virtual RetrievedMemorySet retrieve(const std::string& query, int limit = 5) = 0;

    contracts::PortResult<RetrievedMemorySet> retrieve_result(
        const std::string& query, int limit = 5) {
        return contracts::capture_port_result<RetrievedMemorySet>(
            "memory.retrieve", [&] { return retrieve(query, limit); });
    }
};

} // namespace eu_digital
