#pragma once

#include "core/capability_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eu_digital {

inline constexpr const char* AVATAR_PRESENTATION_SCHEMA_VERSION = "1.0";
inline constexpr const char* AVATAR_PRESENTATION_PROFILE_SOURCE = "procedural";
inline constexpr const char* AVATAR_PRESENTATION_PROFILE_VERSION = "1.0";

enum class AvatarShape { particles, filament, smoke, metaball };

enum class AvatarViewKind { hidden, quiet, notice, question };

enum class AvatarFeedbackAction { correct, defer, silence };

enum class AvatarHealthStatus { healthy, degraded, unavailable };

struct AvatarColor {
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    std::uint8_t alpha{255};
};

struct AvatarProfileOverride {
    bool active{false};
    std::string override_id;
    std::string reason;
    std::string version{AVATAR_PRESENTATION_PROFILE_VERSION};

    void validate() const {
        if (version != AVATAR_PRESENTATION_PROFILE_VERSION) {
            throw std::invalid_argument("avatar override version is unsupported");
        }
        if (active && (override_id.empty() || reason.empty())) {
            throw std::invalid_argument("active avatar override requires id and reason");
        }
        if (!active && (!override_id.empty() || !reason.empty())) {
            throw std::invalid_argument("inactive avatar override cannot carry audit fields");
        }
    }
};

struct AvatarPresentationProfile {
    std::string profile_id{"default-procedural"};
    std::string schema_version{AVATAR_PRESENTATION_SCHEMA_VERSION};
    std::string source{AVATAR_PRESENTATION_PROFILE_SOURCE};
    std::string motif{"neutral_field"};
    std::string profile_version{AVATAR_PRESENTATION_PROFILE_VERSION};
    AvatarShape shape{AvatarShape::particles};
    double density{0.5};
    double turbulence{0.2};
    double glow{0.35};
    std::vector<AvatarColor> palette{{24, 72, 92, 255}, {90, 190, 174, 255}};
    double speed{0.2};
    double cohesion{0.65};
    AvatarProfileOverride override_audit{};

    void validate() const {
        if (profile_id.empty() || motif.empty()) throw std::invalid_argument("avatar profile id and motif are required");
        if (schema_version != AVATAR_PRESENTATION_SCHEMA_VERSION) {
            throw std::invalid_argument("avatar profile schema version is unsupported");
        }
        if (source != AVATAR_PRESENTATION_PROFILE_SOURCE) {
            throw std::invalid_argument("avatar profile source must be procedural");
        }
        if (profile_version != AVATAR_PRESENTATION_PROFILE_VERSION) {
            throw std::invalid_argument("avatar profile version is unsupported");
        }
        validate_unit(density, "density");
        validate_unit(turbulence, "turbulence");
        validate_unit(glow, "glow");
        validate_unit(speed, "speed");
        validate_unit(cohesion, "cohesion");
        if (palette.empty() || palette.size() > 4) {
            throw std::invalid_argument("avatar palette must contain one to four colors");
        }
        override_audit.validate();
    }

private:
    static void validate_unit(double value, const char* field) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            throw std::invalid_argument(std::string("avatar ") + field + " must be between zero and one");
        }
    }
};

struct AvatarViewState {
    std::string view_id;
    AvatarViewKind state{AvatarViewKind::hidden};
    std::optional<std::string> notice_id;
    std::string schema_version{AVATAR_PRESENTATION_SCHEMA_VERSION};
    bool blocks_work{false};
    bool captures_input{false};
    bool has_focus{false};

    void validate() const {
        if (view_id.empty()) throw std::invalid_argument("avatar view id is required");
        if (schema_version != AVATAR_PRESENTATION_SCHEMA_VERSION) {
            throw std::invalid_argument("avatar view schema version is unsupported");
        }
        if (blocks_work || captures_input || has_focus) {
            throw std::invalid_argument("avatar view must not block work, capture input, or receive focus");
        }
        const bool needs_notice = state == AvatarViewKind::notice || state == AvatarViewKind::question;
        if (needs_notice != notice_id.has_value()) {
            throw std::invalid_argument("notice/question view state must match notice_id presence");
        }
        if (notice_id && notice_id->empty()) throw std::invalid_argument("avatar notice id cannot be empty");
    }
};

struct AvatarRenderControls {
    bool consent_granted{false};
    bool global_paused{false};
    bool allow_correct{true};
    bool allow_defer{true};
    bool allow_silence{true};

    void validate() const {
        if (!allow_correct && !allow_defer && !allow_silence) {
            throw std::invalid_argument("at least one avatar feedback control must remain available");
        }
    }
};

struct AvatarRenderQuota {
    std::size_t max_frames_per_window{60};
    std::size_t consumed_frames{0};

    void validate() const {
        if (max_frames_per_window == 0 || max_frames_per_window > 10000) {
            throw std::invalid_argument("avatar frame quota is outside the supported bounds");
        }
        if (consumed_frames > max_frames_per_window) {
            throw std::invalid_argument("avatar consumed frames exceed the quota");
        }
    }

    void reset() { consumed_frames = 0; }

    bool consume() {
        if (consumed_frames >= max_frames_per_window) return false;
        ++consumed_frames;
        return true;
    }
};

struct AvatarRenderHealth {
    AvatarHealthStatus status{AvatarHealthStatus::healthy};
    std::size_t rendered_frames{0};
    std::size_t dropped_frames{0};
    std::string last_reason{"idle"};
};

struct AvatarFeedbackRecord {
    std::string event_id;
    AvatarFeedbackAction action{AvatarFeedbackAction::silence};
    std::optional<std::string> correction;
};

struct AvatarFrame {
    std::uint32_t width{0};
    std::uint32_t height{0};
    bool rendered{false};
    std::string reason;
    std::vector<std::uint32_t> rgba;
};

class ProceduralAvatarRenderer {
public:
    ProceduralAvatarRenderer(AvatarPresentationProfile profile, AvatarViewState view,
                             AvatarRenderControls controls = {}, AvatarRenderQuota quota = {})
        : profile_(std::move(profile)), view_(std::move(view)), controls_(controls), quota_(quota) {
        profile_.validate();
        view_.validate();
        controls_.validate();
        quota_.validate();
    }

    const AvatarPresentationProfile& profile() const { return profile_; }
    const AvatarViewState& view() const { return view_; }
    const AvatarRenderControls& controls() const { return controls_; }
    const AvatarRenderQuota& quota() const { return quota_; }
    const AvatarRenderHealth& health() const { return health_; }
    const std::vector<AvatarFeedbackRecord>& history() const { return history_; }

    void set_view(AvatarViewState view) {
        view.validate();
        view_ = std::move(view);
    }

    void set_consent(bool granted) { controls_.consent_granted = granted; }
    void set_global_pause(bool paused) { controls_.global_paused = paused; }
    void begin_quota_window() { quota_.reset(); }

    void apply_feedback(AvatarFeedbackAction action, std::optional<std::string> correction = std::nullopt) {
        const bool allowed = action == AvatarFeedbackAction::correct ? controls_.allow_correct :
            action == AvatarFeedbackAction::defer ? controls_.allow_defer : controls_.allow_silence;
        if (!allowed) throw std::invalid_argument("avatar feedback control is disabled");
        if (action == AvatarFeedbackAction::correct) {
            if (!correction || correction->empty()) throw std::invalid_argument("correct feedback requires a correction");
        } else if (correction) {
            throw std::invalid_argument("only correct feedback accepts a correction");
        }
        history_.push_back({"avatar-feedback-" + std::to_string(history_.size() + 1), action, std::move(correction)});
        if (action == AvatarFeedbackAction::silence) {
            view_.state = AvatarViewKind::hidden;
            view_.notice_id.reset();
        } else {
            view_.state = AvatarViewKind::quiet;
            view_.notice_id.reset();
        }
        view_.validate();
    }

    AvatarFrame render(std::uint32_t width, std::uint32_t height, std::uint64_t frame_index) {
        if (width == 0 || height == 0 || width > 1024 || height > 1024 ||
            static_cast<std::uint64_t>(width) * height > 1024ULL * 1024ULL) {
            throw std::invalid_argument("avatar frame dimensions exceed the CPU-first bounds");
        }
        if (view_.state == AvatarViewKind::hidden) return skip(width, height, "view_hidden", AvatarHealthStatus::healthy, false);
        if (!controls_.consent_granted) return skip(width, height, "consent_required", AvatarHealthStatus::unavailable, true);
        if (controls_.global_paused) return skip(width, height, "globally_paused", AvatarHealthStatus::degraded, true);
        if (!quota_.consume()) return skip(width, height, "quota_exhausted", AvatarHealthStatus::degraded, true);

        std::vector<std::uint32_t> pixels(static_cast<std::size_t>(width) * height, 0);
        const double visibility = view_.state == AvatarViewKind::quiet ? 0.25 :
            view_.state == AvatarViewKind::notice ? 0.65 : 1.0;
        const double phase = static_cast<double>(frame_index) * profile_.speed * 0.08;
        for (std::uint32_t row = 0; row < height; ++row) {
            for (std::uint32_t column = 0; column < width; ++column) {
                const double x = width == 1 ? 0.0 : (2.0 * static_cast<double>(column) / (width - 1)) - 1.0;
                const double y = height == 1 ? 0.0 : (2.0 * static_cast<double>(row) / (height - 1)) - 1.0;
                const double signal = shape_signal(x, y, phase, frame_index);
                const double intensity = std::clamp(signal * visibility, 0.0, 1.0);
                pixels[static_cast<std::size_t>(row) * width + column] = color_for(intensity);
            }
        }
        ++health_.rendered_frames;
        health_.status = AvatarHealthStatus::healthy;
        health_.last_reason = "rendered";
        return {width, height, true, "rendered", std::move(pixels)};
    }

private:
    static double gaussian(double distance, double width) {
        return std::exp(-(distance * distance) / std::max(width, 1e-6));
    }

    static double clamp_signal(double value) { return std::clamp(value, 0.0, 1.0); }

    double shape_signal(double x, double y, double phase, std::uint64_t frame_index) const {
        const double turbulence = profile_.turbulence *
            0.15 * std::sin((x - y) * 7.0 + phase * 1.7 + static_cast<double>(frame_index % 17));
        double signal = 0.0;
        switch (profile_.shape) {
        case AvatarShape::particles: {
            const int count = 1 + static_cast<int>(profile_.density * 31.0);
            for (int index = 0; index < count; ++index) {
                const double normalized = static_cast<double>(index + 1) / (count + 1.0);
                const double angle = normalized * 6.283185307179586 + phase * (0.5 + profile_.speed);
                const double radius = 0.12 + (1.0 - profile_.cohesion) * 0.58 * normalized;
                const double center_x = std::cos(angle * 1.3 + turbulence) * radius;
                const double center_y = std::sin(angle * 1.7 - turbulence) * radius;
                signal = std::max(signal, gaussian(std::hypot(x - center_x, y - center_y), 0.008 + profile_.density * 0.05));
            }
            break;
        }
        case AvatarShape::filament: {
            const double center = 0.42 * std::sin(y * (2.0 + profile_.turbulence * 4.0) + phase) + turbulence;
            signal = gaussian(x - center, 0.012 + (1.0 - profile_.cohesion) * 0.08);
            break;
        }
        case AvatarShape::smoke: {
            const double drift = 0.25 * std::sin(phase + y * 2.0) + turbulence;
            signal = gaussian(std::hypot(x - drift, y * 0.8), 0.10 + profile_.density * 0.20);
            break;
        }
        case AvatarShape::metaball: {
            for (int index = 0; index < 3; ++index) {
                const double angle = phase * 0.7 + index * 2.094395102393195;
                const double center_x = std::cos(angle) * (0.20 + profile_.cohesion * 0.10);
                const double center_y = std::sin(angle) * (0.20 + profile_.cohesion * 0.10);
                signal += gaussian(std::hypot(x - center_x, y - center_y), 0.06 + profile_.density * 0.12);
            }
            break;
        }
        }
        return clamp_signal(signal * (0.5 + profile_.density * 0.5));
    }

    std::uint32_t color_for(double intensity) const {
        if (intensity <= 0.001) return 0;
        const double palette_position = intensity * static_cast<double>(profile_.palette.size() - 1);
        const auto lower = static_cast<std::size_t>(palette_position);
        const auto upper = std::min(lower + 1, profile_.palette.size() - 1);
        const double fraction = palette_position - static_cast<double>(lower);
        const auto& left = profile_.palette[lower];
        const auto& right = profile_.palette[upper];
        const auto interpolate = [fraction](std::uint8_t first, std::uint8_t second) {
            return static_cast<std::uint8_t>(std::clamp(std::round(first + (second - first) * fraction), 0.0, 255.0));
        };
        const auto alpha = static_cast<std::uint8_t>(std::clamp(
            std::round(intensity * (0.25 + 0.75 * profile_.glow) * 255.0), 0.0, 255.0));
        return (static_cast<std::uint32_t>(interpolate(left.red, right.red)) << 24) |
            (static_cast<std::uint32_t>(interpolate(left.green, right.green)) << 16) |
            (static_cast<std::uint32_t>(interpolate(left.blue, right.blue)) << 8) |
            static_cast<std::uint32_t>(alpha);
    }

    AvatarFrame skip(std::uint32_t width, std::uint32_t height, std::string reason,
                     AvatarHealthStatus status, bool count_as_drop) {
        if (count_as_drop) ++health_.dropped_frames;
        health_.status = status;
        health_.last_reason = reason;
        return {width, height, false, std::move(reason), {}};
    }

    AvatarPresentationProfile profile_;
    AvatarViewState view_;
    AvatarRenderControls controls_;
    AvatarRenderQuota quota_;
    AvatarRenderHealth health_;
    std::vector<AvatarFeedbackRecord> history_;
};

class ProceduralAvatarPlugin final : public CapabilityPlugin {
public:
    ProceduralAvatarPlugin() {
        descriptor_.capability_id = "presentation.procedural_avatar";
        descriptor_.implementation_id = "native.procedural_avatar";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "presentation";
        descriptor_.provides.push_back({"render.avatar_frame", "urn:eu-digital:avatar-frame:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
    }

    const CapabilityDescriptor& descriptor() const override { return descriptor_; }
    void validate_manifest() override {}
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return true; }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

private:
    CapabilityDescriptor descriptor_;
};

}  // namespace eu_digital
