#pragma once

#include "core/event_bus.hpp"
#include "core/contracts/memory_retrieval_result.hpp"
#include <string>

namespace eu_digital {

class IMemoryRetrievalPort {
public:
    virtual ~IMemoryRetrievalPort() = default;

    virtual RetrievedMemorySet retrieve(const std::string& query, int limit = 5) = 0;
};

} // namespace eu_digital
