#pragma once

#include <string>
#include <vector>

namespace eu_digital {

struct RetrievedMemorySet {
    struct MemoryItem {
        std::string memory_id;
        std::string payload;
        double relevance;
    };

    std::vector<MemoryItem> items;
};

} // namespace eu_digital
