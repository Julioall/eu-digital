#pragma once

#include <string>
#include <vector>

namespace eu_digital {

struct CognitiveOutputRequest {
    std::string intent; // "silence", "question", "requested_response", "proactive_suggestion"
    std::string self_constraint_snapshot;
    std::vector<std::string> context_memories;
    std::string prompt_parameters;
};

struct ValidatedDialogueOutput {
    std::string rendered_text;
    std::string status; // "rendered", "malformed", "timeout", "fallback_used", "silence"

    static ValidatedDialogueOutput silence() {
        return {"", "silence"};
    }

    static ValidatedDialogueOutput fallback(const std::string& fallback_text) {
        return {fallback_text, "fallback_used"};
    }

    static ValidatedDialogueOutput success(const std::string& text) {
        return {text, "rendered"};
    }

    static ValidatedDialogueOutput error(const std::string& reason) {
        return {"", reason}; // reason could be "malformed" or "timeout"
    }
};

} // namespace eu_digital
