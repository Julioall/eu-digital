#pragma once

#include "capability_runtime.hpp"
#include "event_bus.hpp"
#include "observation_privacy.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace eu_digital {

enum class RawInputKind { key_down, key_up, mouse_move, mouse_button, clipboard };

struct RawInputEvent {
    RawInputKind kind{RawInputKind::key_down};
    std::uint64_t timestamp_ms{};
    std::uint32_t code{};
    int delta_x{};
    int delta_y{};
    bool control{};
    bool alt{};
    bool shift{};
    std::uint64_t clipboard_length{};
    std::string clipboard_digest;

    static RawInputEvent key_down(std::uint32_t code, std::uint64_t timestamp_ms,
                                  bool control, bool alt, bool shift) {
        return {RawInputKind::key_down, timestamp_ms, code, 0, 0, control, alt, shift, 0, {}};
    }

    static RawInputEvent key_up(std::uint32_t code, std::uint64_t timestamp_ms,
                                bool control, bool alt, bool shift) {
        return {RawInputKind::key_up, timestamp_ms, code, 0, 0, control, alt, shift, 0, {}};
    }

    static RawInputEvent mouse_move(std::uint64_t timestamp_ms, int delta_x, int delta_y) {
        return {RawInputKind::mouse_move, timestamp_ms, 0, delta_x, delta_y, false, false, false, 0, {}};
    }

    static RawInputEvent mouse_button(std::uint64_t timestamp_ms, std::uint32_t button) {
        return {RawInputKind::mouse_button, timestamp_ms, button, 0, 0, false, false, false, 0, {}};
    }

    static RawInputEvent clipboard(std::uint64_t timestamp_ms, std::uint64_t length,
                                   std::string digest) {
        return {RawInputKind::clipboard, timestamp_ms, 0, 0, 0, false, false, false, length, std::move(digest)};
    }
};

struct WindowContext {
    std::uint32_t process_id{};
    std::string process_name;
    std::string window_title;
    std::string application_category;

    bool operator==(const WindowContext&) const = default;
};

struct InputInteractionConfig {
    bool emit_raw_events{false};
    bool capture_key_codes{false};
    std::uint64_t aggregate_window_ms{1000};
    std::uint64_t pause_threshold_ms{1000};
    ObservationPrivacyPolicy privacy_policy{};
};

struct InputInteractionHealth {
    bool paused{false};
    std::uint64_t suppressed_observations{0};
    std::string last_suppression_reason;
};

class InputInteractionSensor {
public:
    using EventSink = std::function<void(const CanonicalEvent&)>;

    InputInteractionSensor(EventSink event_sink, InputInteractionConfig config = {})
        : event_sink_(std::move(event_sink)), config_(config) {
        descriptor_.capability_id = "interaction.input";
        descriptor_.implementation_id = "windows.input_interaction";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "sensor";
        descriptor_.provides.push_back({"observe.input_interaction", "urn:eu-digital:input-interaction:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
        descriptor_.permissions = {"input.hook.read", "clipboard.read"};
    }

    const CapabilityDescriptor& descriptor() const { return descriptor_; }
    const InputInteractionConfig& config() const { return config_; }
    const InputInteractionHealth& health() const { return health_; }
    bool health_check() const { return true; }

    void ingest(const RawInputEvent& input, const WindowContext& context) {
        if (config_.privacy_policy.global_pause) {
            health_.paused = true;
            health_.last_suppression_reason = "global_pause";
            ++health_.suppressed_observations;
            return;
        }
        health_.paused = false;
        if (!config_.privacy_policy.application_allowed(context.process_name)) {
            health_.last_suppression_reason = "application_denylist";
            ++health_.suppressed_observations;
            return;
        }
        WindowContext sanitized_context = context;
        if (sanitized_context.application_category.empty()) {
            sanitized_context.application_category = ObservationPrivacyPolicy::application_category(
                sanitized_context.process_name);
        }
        sanitized_context.window_title = config_.privacy_policy.redact_window_title(
            sanitized_context.process_name, sanitized_context.window_title);
        if (input.kind == RawInputKind::clipboard) {
            if (!config_.privacy_policy.capture_clipboard) {
                health_.last_suppression_reason = "clipboard_disabled";
                ++health_.suppressed_observations;
                return;
            }
            emit_clipboard(input, sanitized_context);
            return;
        }
        if (!aggregate_start_ms_ || input.timestamp_ms < *aggregate_start_ms_ ||
            input.timestamp_ms - *aggregate_start_ms_ >= config_.aggregate_window_ms ||
            (aggregate_context_ && *aggregate_context_ != sanitized_context)) {
            flush();
            aggregate_start_ms_ = input.timestamp_ms;
            aggregate_context_ = sanitized_context;
        }
        if (!aggregate_context_) aggregate_context_ = sanitized_context;
        if (config_.emit_raw_events) emit_raw(input, sanitized_context);
        update_aggregate(input);
        last_timestamp_ms_ = input.timestamp_ms;
    }

    bool flush() {
        if (!aggregate_start_ms_) return false;
        const auto context = aggregate_context_.value_or(WindowContext{});
        const std::uint64_t duration_ms = last_timestamp_ms_ >= *aggregate_start_ms_
            ? last_timestamp_ms_ - *aggregate_start_ms_ : 0;
        const double rate = duration_ms == 0
            ? static_cast<double>(aggregate_.key_down_count) * 60'000.0 /
                static_cast<double>(std::max<std::uint64_t>(config_.aggregate_window_ms, 1))
            : static_cast<double>(aggregate_.key_down_count) * 60'000.0 /
                static_cast<double>(duration_ms);
        std::ostringstream payload;
        payload << base_payload(context)
                << ",\"duration_ms\":" << duration_ms
                << ",\"key_down_count\":" << aggregate_.key_down_count
                << ",\"key_up_count\":" << aggregate_.key_up_count
                << ",\"mouse_move_count\":" << aggregate_.mouse_move_count
                << ",\"mouse_button_count\":" << aggregate_.mouse_button_count
                << ",\"mouse_distance\":" << aggregate_.mouse_distance
                << ",\"shortcut_count\":" << aggregate_.shortcut_count
                << ",\"pause_count\":" << aggregate_.pause_count
                << ",\"typing_rate_per_minute\":" << rate << "}";
        emit("input.aggregate", payload.str(), last_timestamp_ms_);
        aggregate_ = {};
        aggregate_start_ms_.reset();
        aggregate_context_.reset();
        return true;
    }

private:
    struct Aggregate {
        std::uint64_t key_down_count{};
        std::uint64_t key_up_count{};
        std::uint64_t mouse_move_count{};
        std::uint64_t mouse_button_count{};
        std::uint64_t mouse_distance{};
        std::uint64_t shortcut_count{};
        std::uint64_t pause_count{};
    };

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    static const char* kind_name(RawInputKind kind) {
        switch (kind) {
        case RawInputKind::key_down: return "key_down";
        case RawInputKind::key_up: return "key_up";
        case RawInputKind::mouse_move: return "mouse_move";
        case RawInputKind::mouse_button: return "mouse_button";
        case RawInputKind::clipboard: return "clipboard";
        }
        return "unknown";
    }

    std::string base_payload(const WindowContext& context) const {
        std::ostringstream payload;
        payload << "{\"payload_schema_version\":\"1.0\",\"window_context\":{\"available\":"
                << (context.process_id != 0 ? "true" : "false")
                << ",\"process_id\":" << context.process_id
                << ",\"process_name\":\"" << json_escape(context.process_name)
                << "\",\"application_category\":\""
                << json_escape(context.application_category)
                << "\",\"window_title\":\"" << json_escape(context.window_title)
                << "\",\"text_content_observed\":"
                << (config_.privacy_policy.capture_window_title ? "true" : "false") << "}";
        return payload.str();
    }

    void emit_raw(const RawInputEvent& input, const WindowContext& context) {
        std::ostringstream payload;
        payload << base_payload(context)
                << ",\"kind\":\"" << kind_name(input.kind) << "\"";
        if (input.kind == RawInputKind::key_down || input.kind == RawInputKind::key_up) {
            payload << ",\"key_code\":" << (config_.capture_key_codes ? input.code : 0)
                    << ",\"control\":" << (input.control ? "true" : "false")
                    << ",\"alt\":" << (input.alt ? "true" : "false")
                    << ",\"shift\":" << (input.shift ? "true" : "false");
        } else if (input.kind == RawInputKind::mouse_move) {
            payload << ",\"delta_x\":" << input.delta_x << ",\"delta_y\":" << input.delta_y;
        } else if (input.kind == RawInputKind::mouse_button) {
            payload << ",\"button\":" << input.code;
        }
        payload << "}";
        const std::string event_type = input.kind == RawInputKind::key_down || input.kind == RawInputKind::key_up
            ? "input.key" : "input.pointer";
        emit(event_type, payload.str(), input.timestamp_ms);
    }

    void emit_clipboard(const RawInputEvent& input, const WindowContext& context) {
        std::ostringstream payload;
        payload << base_payload(context)
                << ",\"content_length\":" << input.clipboard_length
                << ",\"content_digest\":\"" << json_escape(input.clipboard_digest)
                << "\",\"text_content_observed\":true}";
        emit("input.clipboard", payload.str(), input.timestamp_ms);
    }

    void update_aggregate(const RawInputEvent& input) {
        if (input.kind == RawInputKind::key_down) {
            ++aggregate_.key_down_count;
            if (input.control || input.alt || input.shift) ++aggregate_.shortcut_count;
            if (last_key_down_ms_ && input.timestamp_ms > *last_key_down_ms_ &&
                input.timestamp_ms - *last_key_down_ms_ >= config_.pause_threshold_ms) {
                ++aggregate_.pause_count;
            }
            last_key_down_ms_ = input.timestamp_ms;
        } else if (input.kind == RawInputKind::key_up) {
            ++aggregate_.key_up_count;
        } else if (input.kind == RawInputKind::mouse_move) {
            ++aggregate_.mouse_move_count;
            aggregate_.mouse_distance += static_cast<std::uint64_t>(std::abs(input.delta_x)) +
                                          static_cast<std::uint64_t>(std::abs(input.delta_y));
        } else if (input.kind == RawInputKind::mouse_button) {
            ++aggregate_.mouse_button_count;
        }
    }

    void emit(const std::string& event_type, const std::string& payload, std::uint64_t timestamp_ms) {
        if (!event_sink_) return;
        CanonicalEvent event;
        event.event_id = "input-interaction-" + std::to_string(next_event_id_++);
        event.source = "input_interaction_sensor";
        event.event_type = event_type;
        event.payload = payload;
        event.monotonic_ns = timestamp_ms * 1'000'000;
        event_sink_(event);
    }

    EventSink event_sink_;
    InputInteractionConfig config_;
    CapabilityDescriptor descriptor_;
    std::optional<std::uint64_t> aggregate_start_ms_;
    std::optional<std::uint64_t> last_key_down_ms_;
    std::uint64_t last_timestamp_ms_{0};
    std::optional<WindowContext> aggregate_context_;
    Aggregate aggregate_;
    InputInteractionHealth health_;
    std::uint64_t next_event_id_{1};
};

class InputInteractionPlugin final : public CapabilityPlugin {
public:
    InputInteractionPlugin(InputInteractionSensor::EventSink event_sink,
                           InputInteractionConfig config = {})
        : sensor_(std::move(event_sink), config) {}

    const CapabilityDescriptor& descriptor() const override { return sensor_.descriptor(); }
    void validate_manifest() override {
        if (!descriptor().valid()) throw CapabilityLifecycleError("invalid input interaction descriptor");
    }
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return sensor_.health_check(); }
    void start() override {}
    void drain() override { sensor_.flush(); }
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

    void ingest(const RawInputEvent& input, const WindowContext& context) { sensor_.ingest(input, context); }
    bool flush() { return sensor_.flush(); }

private:
    InputInteractionSensor sensor_;
};

class WindowsInputCaptureAdapter {
public:
    using Sink = std::function<void(const RawInputEvent&, const WindowContext&)>;

    explicit WindowsInputCaptureAdapter(Sink sink, bool capture_window_title = false)
        : sink_(std::move(sink)), capture_window_title_(capture_window_title) {}

    bool captures_window_title() const noexcept { return capture_window_title_; }

    bool start() {
#ifdef _WIN32
        if (active_ != nullptr && active_ != this) return false;
        active_ = this;
        keyboard_hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &keyboard_proc, nullptr, 0);
        mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, &mouse_proc, nullptr, 0);
        if (keyboard_hook_ == nullptr || mouse_hook_ == nullptr) {
            stop();
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void stop() {
#ifdef _WIN32
        if (keyboard_hook_ != nullptr) {
            UnhookWindowsHookEx(keyboard_hook_);
            keyboard_hook_ = nullptr;
        }
        if (mouse_hook_ != nullptr) {
            UnhookWindowsHookEx(mouse_hook_);
            mouse_hook_ = nullptr;
        }
        if (active_ == this) active_ = nullptr;
#endif
    }

    bool pump_once() {
#ifdef _WIN32
        MSG message{};
        bool processed = false;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            processed = true;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return processed;
#else
        return false;
#endif
    }

    void keyboard_event(std::uint32_t key_code, bool down, std::uint64_t timestamp_ms,
                        bool control, bool alt, bool shift, WindowContext context) {
        if (!sink_) return;
        const RawInputEvent event = down
            ? RawInputEvent::key_down(key_code, timestamp_ms, control, alt, shift)
            : RawInputEvent::key_up(key_code, timestamp_ms, control, alt, shift);
        sink_(event, context);
    }

    void mouse_move(int delta_x, int delta_y, std::uint64_t timestamp_ms, WindowContext context) {
        if (sink_) sink_(RawInputEvent::mouse_move(timestamp_ms, delta_x, delta_y), context);
    }

    void mouse_button(std::uint32_t button, std::uint64_t timestamp_ms, WindowContext context) {
        if (sink_) sink_(RawInputEvent::mouse_button(timestamp_ms, button), context);
    }

    void clipboard_event(std::uint64_t length, std::string digest,
                         std::uint64_t timestamp_ms, WindowContext context) {
        if (sink_) sink_(RawInputEvent::clipboard(timestamp_ms, length, std::move(digest)), context);
    }

private:
#ifdef _WIN32
    WindowContext foreground_context() const {
        WindowContext context;
        const HWND window = GetForegroundWindow();
        if (window == nullptr) return context;
        DWORD process_id = 0;
        GetWindowThreadProcessId(window, &process_id);
        context.process_id = process_id;
        context.process_name = process_name_for_id(process_id);
        if (capture_window_title_) {
            const int title_length = GetWindowTextLengthW(window);
            if (title_length > 0) {
                std::vector<wchar_t> buffer(static_cast<std::size_t>(title_length) + 1);
                GetWindowTextW(window, buffer.data(), static_cast<int>(buffer.size()));
                context.window_title = wide_to_utf8(std::wstring(buffer.data()));
            }
        }
        return context;
    }

    static std::string wide_to_utf8(const std::wstring& value) {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                             nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                            result.data(), size, nullptr, nullptr);
        return result;
    }

    static std::string process_name_for_id(DWORD process_id) {
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
        if (process == nullptr) return {};
        std::vector<wchar_t> buffer(1024);
        DWORD length = static_cast<DWORD>(buffer.size());
        const bool read = QueryFullProcessImageNameW(process, 0, buffer.data(), &length) != FALSE;
        CloseHandle(process);
        if (!read) return {};
        std::wstring path(buffer.data(), length);
        const auto separator = path.find_last_of(L"\\/");
        return wide_to_utf8(separator == std::wstring::npos ? path : path.substr(separator + 1));
    }

    static LRESULT CALLBACK keyboard_proc(int code, WPARAM message, LPARAM data) {
        if (active_ != nullptr && code == HC_ACTION) {
            const auto* keyboard = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
            const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            const bool up = message == WM_KEYUP || message == WM_SYSKEYUP;
            if (down || up) {
                active_->keyboard_event(
                    keyboard->vkCode, down, GetTickCount64(),
                    (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0,
                    (GetAsyncKeyState(VK_MENU) & 0x8000) != 0,
                    (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0,
                    active_->foreground_context());
            }
        }
        return CallNextHookEx(nullptr, code, message, data);
    }

    static LRESULT CALLBACK mouse_proc(int code, WPARAM message, LPARAM data) {
        if (active_ != nullptr && code == HC_ACTION) {
            const auto* mouse = reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
            const WindowContext context = active_->foreground_context();
            const std::uint64_t timestamp = GetTickCount64();
            if (message == WM_MOUSEMOVE) {
                const int delta_x = active_->last_mouse_position_
                    ? mouse->pt.x - active_->last_mouse_position_->x : 0;
                const int delta_y = active_->last_mouse_position_
                    ? mouse->pt.y - active_->last_mouse_position_->y : 0;
                active_->last_mouse_position_ = mouse->pt;
                active_->mouse_move(delta_x, delta_y, timestamp, context);
            } else if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                       message == WM_MBUTTONDOWN) {
                const std::uint32_t button = message == WM_LBUTTONDOWN ? 1 :
                    (message == WM_RBUTTONDOWN ? 2 : 3);
                active_->mouse_button(button, timestamp, context);
            }
        }
        return CallNextHookEx(nullptr, code, message, data);
    }

    inline static WindowsInputCaptureAdapter* active_{nullptr};
    inline static HHOOK keyboard_hook_{nullptr};
    inline static HHOOK mouse_hook_{nullptr};
    std::optional<POINT> last_mouse_position_;
#endif

    Sink sink_;
    bool capture_window_title_{false};
};

}  // namespace eu_digital
