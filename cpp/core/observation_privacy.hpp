#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace eu_digital {

struct ObservationPrivacyPolicy {
    static std::vector<std::string> mandatory_denylist() {
        return {"keepass", "1password", "bitwarden", "lastpass", "dashlane",
                "password", "private", "incognito", "inprivate", "tor"};
    }

    std::string policy_id{"capture.local.low-risk"};
    std::string policy_version{"1"};
    bool capture_window_title{false};
    bool capture_clipboard{false};
    bool global_pause{false};
    std::vector<std::string> allowlist;
    std::vector<std::string> denylist{mandatory_denylist()};
    std::string redaction_version{"length-only-v1"};

    bool valid() const {
        if (policy_id.empty() || policy_version.empty() || redaction_version.empty()) return false;
        for (const auto& required : mandatory_denylist()) {
            if (std::none_of(denylist.begin(), denylist.end(), [&](const std::string& value) {
                    return lower(value) == lower(required);
                })) return false;
        }
        return true;
    }

    bool application_allowed(const std::string& process_name) const {
        if (!valid()) return false;
        const auto normalized = lower(process_name);
        for (const auto& denied : denylist) {
            const auto token = lower(denied);
            if (token == "tor") {
                if (normalized == "tor" || normalized == "tor.exe" || normalized.starts_with("torbrowser")) return false;
            } else if (!token.empty() && normalized.find(token) != std::string::npos) {
                return false;
            }
        }
        if (!allowlist.empty()) {
            return std::any_of(allowlist.begin(), allowlist.end(), [&](const std::string& allowed) {
                return lower(allowed) == normalized;
            });
        }
        return !normalized.empty();
    }

    std::string redact_window_title(const std::string& process_name,
                                    const std::string& title) const {
        if (!capture_window_title || !application_allowed(process_name) || title.empty()) return {};
        return "[redacted:length=" + std::to_string(title.size()) + "]";
    }

    static std::string application_category(const std::string& process_name) {
        const auto normalized = lower(process_name);
        if (contains_any(normalized, {"chrome", "msedge", "firefox", "brave", "opera", "browser"})) return "browser";
        if (contains_any(normalized, {"code", "devenv", "notepad", "vim", "emacs", "word", "excel"})) return "editor";
        if (contains_any(normalized, {"terminal", "powershell", "pwsh", "cmd", "bash", "wsl"})) return "terminal";
        if (contains_any(normalized, {"explorer", "finder"})) return "file_manager";
        return normalized.empty() ? "unknown" : "other";
    }

private:
    static std::string lower(const std::string& value) {
        std::string normalized = value;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
        return normalized;
    }

    static bool contains_any(const std::string& value, const std::vector<std::string>& tokens) {
        return std::any_of(tokens.begin(), tokens.end(), [&](const std::string& token) {
            return value.find(token) != std::string::npos;
        });
    }
};

}  // namespace eu_digital
