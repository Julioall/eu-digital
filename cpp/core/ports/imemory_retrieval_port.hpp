#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/cognitive_port_requests.hpp"
#include "core/contracts/memory_retrieval_result.hpp"
#include "core/contracts/port_result.hpp"
#include <stdexcept>
#include <string>

namespace eu_digital {

class IMemoryRetrievalPort {
public:
    virtual ~IMemoryRetrievalPort() = default;

    virtual RetrievedMemorySet retrieve(const std::string& query, int limit = 5) = 0;

    virtual contracts::MemoryRetrievalResponse retrieve_memory(
        const contracts::MemoryRetrievalRequest&) {
        throw std::logic_error("memory retrieval requests are not implemented");
    }

    contracts::PortResult<RetrievedMemorySet> retrieve_result(
        const std::string& query, int limit = 5) {
        return contracts::capture_port_result<RetrievedMemorySet>(
            "memory.retrieve", [&] { return retrieve(query, limit); });
    }

    contracts::PortResult<contracts::MemoryRetrievalResponse> retrieve_memory_result(
        const contracts::MemoryRetrievalRequest& request) {
        return contracts::capture_port_result<contracts::MemoryRetrievalResponse>(
            "memory.retrieve_memory", [&] { return retrieve_memory(request); });
    }
};

} // namespace eu_digital
