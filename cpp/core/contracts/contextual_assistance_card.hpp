#pragma once

#include <string>

namespace eu_digital {

/// A card of contextual assistance surfaced by the cognitive cycle.
/// Displayed in the tray widget when the system has a relevant observation,
/// suggestion or question for the user.
struct ContextualAssistanceCard {
    std::string card_id;
    std::string title;            // e.g., "Você está programando há 2h"
    std::string body;             // e.g., "Considere uma pausa..."
    std::string action_label;     // e.g., "Lembrar em 15min"
    std::string card_type;        // "suggestion" | "observation" | "question"
    double relevance_score = 0.0;

    bool valid() const {
        return !card_id.empty() && !title.empty();
    }
};

} // namespace eu_digital
