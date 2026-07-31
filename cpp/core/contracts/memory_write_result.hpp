#pragma once

#include <string>

namespace eu_digital {

struct MemoryWriteResult {
    bool success;
    std::string memory_id;
    std::string error_message;

    static MemoryWriteResult ok(std::string id) {
        return {true, std::move(id), ""};
    }

    static MemoryWriteResult fail(std::string err) {
        return {false, "", std::move(err)};
    }
};

} // namespace eu_digital
