#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace eu_digital {

struct ScreenRegion {
    int x{};
    int y{};
    int width{};
    int height{};

    bool operator==(const ScreenRegion&) const = default;

    bool structurally_valid() const { return x >= 0 && y >= 0 && width > 0 && height > 0; }
};

struct ScreenOcrRequest {
    std::string request_id;
    std::optional<ScreenRegion> region_of_interest;
};

enum class ScreenOcrConsentState { not_granted, granted, revoked };
enum class ScreenOcrCaptureMode { disabled, explicit_enable, on_demand };

struct ScreenOcrCapturePolicy {
    std::string schema_version{"1.0"};
    std::string policy_id{"screen.ocr.local.default"};
    std::string policy_version{"1"};
    std::string sensor_id{"screen.ocr"};
    std::string purpose{"screen_text_observation"};
    ScreenOcrConsentState consent_state{ScreenOcrConsentState::not_granted};
    ScreenOcrCaptureMode capture_mode{ScreenOcrCaptureMode::disabled};
    std::optional<std::string> request_id;
    bool global_pause{false};
    std::optional<ScreenRegion> region_of_interest;
    std::string redaction_version{"length-only-v1"};
    std::uint32_t text_retention_days{7};
    std::uint32_t visual_retention_days{30};
    std::string event_payload_mode{"reference-and-hash-only"};

    bool valid() const {
        if (schema_version != "1.0" || policy_id.empty() || policy_version.empty() ||
            sensor_id != "screen.ocr" || purpose != "screen_text_observation" ||
            redaction_version != "length-only-v1" || text_retention_days == 0 ||
            text_retention_days > 7 || visual_retention_days == 0 || visual_retention_days > 30 ||
            text_retention_days >= visual_retention_days ||
            event_payload_mode != "reference-and-hash-only") return false;
        if (region_of_interest && !region_of_interest->structurally_valid()) return false;
        if (capture_mode == ScreenOcrCaptureMode::explicit_enable) {
            return consent_state == ScreenOcrConsentState::granted && !request_id.has_value();
        }
        if (capture_mode == ScreenOcrCaptureMode::on_demand) {
            return consent_state == ScreenOcrConsentState::granted && request_id.has_value() &&
                !request_id->empty();
        }
        return capture_mode == ScreenOcrCaptureMode::disabled && !request_id.has_value();
    }

    bool allows_capture(const std::optional<ScreenOcrRequest>& request) const {
        if (!valid() || global_pause || consent_state != ScreenOcrConsentState::granted) return false;
        if (capture_mode == ScreenOcrCaptureMode::explicit_enable) return true;
        if (capture_mode != ScreenOcrCaptureMode::on_demand || !request || request->request_id.empty()) {
            return false;
        }
        return request_id && *request_id == request->request_id;
    }

    const char* authorization_mode() const {
        return capture_mode == ScreenOcrCaptureMode::explicit_enable ? "explicit" : "on_demand";
    }

    std::optional<ScreenRegion> region_for(const std::optional<ScreenOcrRequest>& request) const {
        if (request && request->region_of_interest) return request->region_of_interest;
        return region_of_interest;
    }
};

}  // namespace eu_digital
