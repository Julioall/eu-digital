#pragma once

#include <string>

namespace eu_digital {

struct CognitiveDecision {
    bool success;
    std::string intent; // e.g., "silence", "action", "question", "proactive_suggestion"
    std::string reason;
    std::string target_action;
    std::string error_message;

    static CognitiveDecision ok(std::string intent, std::string reason = "", std::string target_action = "") {
        return {true, std::move(intent), std::move(reason), std::move(target_action), ""};
    }

    static CognitiveDecision fail(std::string err) {
        return {false, "silence", "", "", std::move(err)};
    }
};

} // namespace eu_digital
