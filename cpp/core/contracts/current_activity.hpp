#pragma once

#include <string>
#include <vector>

namespace eu_digital {

/// Represents the user's currently detected activity, inferred from sensor observations.
/// This is what the tray widget displays as the primary "companion" view.
struct CurrentActivity {
    std::string activity_id;
    std::string description;      // e.g., "Escrevendo código no VS Code"
    std::string application;      // e.g., "Code.exe"
    std::string started_at;       // ISO 8601
    double confidence = 0.0;      // [0.0, 1.0]
    std::vector<std::string> related_memories;

    bool valid() const {
        return !activity_id.empty() && !description.empty();
    }
};

} // namespace eu_digital
