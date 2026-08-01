#pragma once

#include <string>

namespace eu_digital {

struct CognitiveCycleResult {
    std::string event_id;
    std::string correlation_id;
    std::string intent;
    std::string reason;
    std::string activity_id;
    std::string card_id;
    std::string payload_text;

    bool valid() const {
        return !event_id.empty() && !intent.empty();
    }
};

} // namespace eu_digital
