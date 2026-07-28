#pragma once

#include "capability_runtime.hpp"
#include "event_bus.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <tlhelp32.h>
#include <windows.h>
#endif

namespace eu_digital {

struct ProcessInfo {
    std::uint32_t process_id{};
    std::string process_name;

    bool operator==(const ProcessInfo&) const = default;
};

struct WindowInfo {
    std::uint32_t process_id{};
    std::string process_name;
    std::string window_title;

    bool operator==(const WindowInfo&) const = default;
};

struct SystemActivitySnapshot {
    std::optional<WindowInfo> active_window;
    std::map<std::uint32_t, ProcessInfo> processes;
};

class SystemActivityAdapter {
public:
    virtual ~SystemActivityAdapter() = default;
    virtual bool capture(SystemActivitySnapshot& output) = 0;
    virtual bool reconnect() = 0;
    virtual std::string last_error() const = 0;
};

struct SystemActivityConfig {
    std::uint32_t poll_interval_ms{100};
    double cpu_budget_percent{5.0};
    std::uint32_t max_reconnect_attempts{1};
};

struct SystemActivityHealth {
    bool available{false};
    bool permission_denied{false};
    std::uint32_t consecutive_failures{0};
    double average_cpu_percent{0.0};
    bool cpu_budget_exceeded{false};
    std::string last_error;
};

class SystemActivitySensor {
public:
    using EventSink = std::function<void(const CanonicalEvent&)>;

    SystemActivitySensor(SystemActivityAdapter& adapter, EventSink event_sink,
                         SystemActivityConfig config = {})
        : adapter_(adapter), event_sink_(std::move(event_sink)), config_(config) {
        descriptor_.capability_id = "system.activity";
        descriptor_.implementation_id = "windows.system_activity";
        descriptor_.implementation_version = "1.0.0";
        descriptor_.kind = "sensor";
        descriptor_.provides.push_back({"observe.system_activity", "urn:eu-digital:system-activity:1"});
        descriptor_.supports_hot_plug = true;
        descriptor_.supports_checkpoint = false;
        descriptor_.permissions = {"process.enumeration", "window.focus.read"};
    }

    const CapabilityDescriptor& descriptor() const { return descriptor_; }
    const SystemActivityConfig& config() const { return config_; }
    const SystemActivityHealth& health() const { return health_; }
    bool health_check() const {
        return health_.available && !health_.cpu_budget_exceeded && health_.consecutive_failures == 0;
    }

    bool poll() {
        const auto started = std::chrono::steady_clock::now();
        SystemActivitySnapshot current;
        bool captured = adapter_.capture(current);
        for (std::uint32_t attempt = 0; !captured && attempt < config_.max_reconnect_attempts; ++attempt) {
            if (!adapter_.reconnect()) break;
            captured = adapter_.capture(current);
        }

        if (!captured) {
            health_.available = false;
            ++health_.consecutive_failures;
            health_.last_error = adapter_.last_error();
            health_.permission_denied = contains_permission_error(health_.last_error);
            update_cpu(started);
            return false;
        }

        health_.available = true;
        health_.permission_denied = false;
        health_.consecutive_failures = 0;
        health_.last_error.clear();
        if (!previous_) {
            previous_ = std::move(current);
            update_cpu(started);
            return true;
        }

        emit_window_change(previous_->active_window, current.active_window);
        for (const auto& [process_id, process] : current.processes) {
            if (!previous_->processes.contains(process_id)) {
                emit_process_event("system.process_started", process);
            }
        }
        for (const auto& [process_id, process] : previous_->processes) {
            if (!current.processes.contains(process_id)) {
                emit_process_event("system.process_ended", process);
            }
        }
        previous_ = std::move(current);
        update_cpu(started);
        return true;
    }

private:
    static bool contains_permission_error(const std::string& error) {
        return error.find("permission") != std::string::npos || error.find("access") != std::string::npos;
    }

    static std::string json_escape(const std::string& value) {
        std::string escaped;
        escaped.reserve(value.size());
        for (const char character : value) {
            if (character == '\\' || character == '"') escaped.push_back('\\');
            escaped.push_back(character);
        }
        return escaped;
    }

    static std::uint64_t monotonic_now_ns() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    void emit_window_change(const std::optional<WindowInfo>& previous,
                            const std::optional<WindowInfo>& current) {
        if (previous == current || !current) return;
        std::ostringstream payload;
        payload << "{\"process_id\":" << current->process_id
                << ",\"process_name\":\"" << json_escape(current->process_name)
                << "\",\"window_title\":\"" << json_escape(current->window_title) << "\"}";
        emit("system.window_focus_changed", payload.str());
    }

    void emit_process_event(const std::string& event_type, const ProcessInfo& process) {
        std::ostringstream payload;
        payload << "{\"process_id\":" << process.process_id
                << ",\"process_name\":\"" << json_escape(process.process_name) << "\"}";
        emit(event_type, payload.str());
    }

    void emit(const std::string& event_type, const std::string& payload) {
        if (!event_sink_) return;
        CanonicalEvent event;
        event.event_id = "system-activity-" + std::to_string(next_event_id_++);
        event.source = "system_activity_sensor";
        event.event_type = event_type;
        event.payload = payload;
        event.monotonic_ns = monotonic_now_ns();
        event_sink_(event);
    }

    void update_cpu(const std::chrono::steady_clock::time_point started) {
        const auto elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count();
        const double interval_ns = static_cast<double>(std::max<std::uint32_t>(config_.poll_interval_ms, 1)) * 1'000'000.0;
        const double sample = static_cast<double>(elapsed_ns) / interval_ns * 100.0;
        ++poll_count_;
        average_cpu_percent_ += (sample - average_cpu_percent_) / static_cast<double>(poll_count_);
        health_.average_cpu_percent = average_cpu_percent_;
        health_.cpu_budget_exceeded = health_.average_cpu_percent > config_.cpu_budget_percent;
    }

    SystemActivityAdapter& adapter_;
    EventSink event_sink_;
    SystemActivityConfig config_;
    CapabilityDescriptor descriptor_;
    SystemActivityHealth health_;
    std::optional<SystemActivitySnapshot> previous_;
    std::uint64_t next_event_id_{1};
    std::uint64_t poll_count_{0};
    double average_cpu_percent_{0.0};
};

class SystemActivityPlugin final : public CapabilityPlugin {
public:
    SystemActivityPlugin(SystemActivityAdapter& adapter, SystemActivitySensor::EventSink event_sink,
                         SystemActivityConfig config = {})
        : sensor_(adapter, std::move(event_sink), config) {}

    const CapabilityDescriptor& descriptor() const override { return sensor_.descriptor(); }
    void validate_manifest() override {
        if (!descriptor().valid()) throw CapabilityLifecycleError("invalid system activity descriptor");
    }
    void configure() override {}
    void initialize() override {}
    void calibrate() override {}
    bool health_check() override { return sensor_.poll() && sensor_.health_check(); }
    void start() override {}
    void drain() override {}
    std::map<std::string, std::string> checkpoint() override { return {}; }
    void stop() override {}
    void uninstall() override {}

    bool poll() { return sensor_.poll(); }
    const SystemActivityHealth& health() const { return sensor_.health(); }

private:
    SystemActivitySensor sensor_;
};

class WindowsSystemActivityAdapter final : public SystemActivityAdapter {
public:
    bool capture(SystemActivitySnapshot& output) override {
#ifdef _WIN32
        output = {};
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        const HANDLE process_snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (process_snapshot == INVALID_HANDLE_VALUE) {
            last_error_ = "permission denied: process enumeration";
            return false;
        }
        if (Process32FirstW(process_snapshot, &entry)) {
            do {
                output.processes.emplace(entry.th32ProcessID,
                                         ProcessInfo{entry.th32ProcessID, wide_to_utf8(entry.szExeFile)});
            } while (Process32NextW(process_snapshot, &entry));
        }
        CloseHandle(process_snapshot);

        const HWND foreground = GetForegroundWindow();
        if (foreground != nullptr) {
            DWORD process_id = 0;
            GetWindowThreadProcessId(foreground, &process_id);
            std::wstring title;
            const int title_length = GetWindowTextLengthW(foreground);
            if (title_length > 0) {
                std::vector<wchar_t> buffer(static_cast<std::size_t>(title_length) + 1);
                GetWindowTextW(foreground, buffer.data(), static_cast<int>(buffer.size()));
                title.assign(buffer.data());
            }
            const auto process = output.processes.find(process_id);
            output.active_window = WindowInfo{
                process_id,
                process == output.processes.end() ? std::string{} : process->second.process_name,
                wide_to_utf8(title),
            };
        }
        last_error_.clear();
        return true;
#else
        output = {};
        last_error_ = "windows adapter unavailable on this platform";
        return false;
#endif
    }

    bool reconnect() override {
#ifdef _WIN32
        last_error_.clear();
        return true;
#else
        return false;
#endif
    }

    std::string last_error() const override { return last_error_; }

private:
#ifdef _WIN32
    static std::string wide_to_utf8(const std::wstring& value) {
        if (value.empty()) return {};
        const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                             nullptr, 0, nullptr, nullptr);
        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                            result.data(), size, nullptr, nullptr);
        return result;
    }
#endif

    std::string last_error_;
};

}  // namespace eu_digital
