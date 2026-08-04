#include "shell/desktop_runtime_lifecycle.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eu_digital {
namespace {

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const unsigned char character : value) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character < 0x20) {
                std::ostringstream encoded;
                encoded << "\\u" << std::hex << std::setw(4)
                        << std::setfill('0') << static_cast<int>(character);
                escaped += encoded.str();
            } else {
                escaped.push_back(static_cast<char>(character));
            }
        }
    }
    return escaped;
}

void replace_file(const std::filesystem::path& temporary,
                  const std::filesystem::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.wstring().c_str(), destination.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto error = GetLastError();
        std::filesystem::remove(temporary);
        throw PrivacyStorageError("cannot commit desktop session marker: " +
                                  std::to_string(error));
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw PrivacyStorageError("cannot commit desktop session marker: " +
                                  error.message());
    }
#endif
}

}  // namespace

const std::vector<DesktopConsentPair>& desktop_sensor_consents() {
    static const std::vector<DesktopConsentPair> pairs{
        {kSystemActivitySensorId, kSystemActivityPurpose},
        {kInputInteractionSensorId, kInputInteractionPurpose},
    };
    return pairs;
}

DesktopConsentStore::DesktopConsentStore(std::filesystem::path path)
    : path_(std::move(path)) {}

bool DesktopConsentStore::load() {
    ledger_ = ConsentLedger{};
    loaded_cleanly_ = true;
    error_code_.clear();
    if (!std::filesystem::exists(path_)) return true;
    try {
        ledger_ = ConsentLedger::load_encrypted(path_);
        return true;
    } catch (const std::exception&) {
        ledger_ = ConsentLedger{};
        loaded_cleanly_ = false;
        error_code_ = "consent_ledger_unreadable";
        return false;
    }
}

bool DesktopConsentStore::loaded_cleanly() const noexcept {
    return loaded_cleanly_;
}

const std::string& DesktopConsentStore::error_code() const noexcept {
    return error_code_;
}

bool DesktopConsentStore::capture_allowed(const std::string& sensor_id,
                                          const std::string& purpose) const {
    return loaded_cleanly_ && ledger_.capture_allowed(sensor_id, purpose).allowed;
}

bool DesktopConsentStore::any_granted() const {
    return std::ranges::any_of(desktop_sensor_consents(), [this](const auto& pair) {
        return capture_allowed(pair.sensor_id, pair.purpose);
    });
}

bool DesktopConsentStore::all_granted() const {
    return std::ranges::all_of(desktop_sensor_consents(), [this](const auto& pair) {
        return capture_allowed(pair.sensor_id, pair.purpose);
    });
}

bool DesktopConsentStore::paused() const noexcept {
    return ledger_.global_paused();
}

void DesktopConsentStore::grant(const std::string& sensor_id,
                                const std::string& purpose,
                                const std::string& decided_at) {
    ledger_.grant(sensor_id, purpose, kDesktopConsentVersion, decided_at);
    loaded_cleanly_ = true;
    error_code_.clear();
    persist();
}

void DesktopConsentStore::revoke(const std::string& sensor_id,
                                 const std::string& purpose,
                                 const std::string& decided_at) {
    ledger_.revoke(sensor_id, purpose, kDesktopConsentVersion, decided_at);
    persist();
}

void DesktopConsentStore::grant_all(const std::string& decided_at) {
    for (const auto& pair : desktop_sensor_consents()) {
        ledger_.grant(pair.sensor_id, pair.purpose, kDesktopConsentVersion,
                      decided_at);
    }
    loaded_cleanly_ = true;
    error_code_.clear();
    persist();
}

void DesktopConsentStore::revoke_all(const std::string& decided_at) {
    for (const auto& pair : desktop_sensor_consents()) {
        ledger_.revoke(pair.sensor_id, pair.purpose, kDesktopConsentVersion,
                       decided_at);
    }
    persist();
}

void DesktopConsentStore::set_paused(bool paused_value) {
    if (paused_value) {
        ledger_.pause_global();
    } else {
        ledger_.resume_global();
    }
    persist();
}

const ConsentLedger& DesktopConsentStore::ledger() const noexcept {
    return ledger_;
}

void DesktopConsentStore::persist() {
    ledger_.save_encrypted(path_);
}

DesktopSessionMarker::DesktopSessionMarker(std::filesystem::path path)
    : path_(std::move(path)) {}

bool DesktopSessionMarker::begin(const std::string& session_id,
                                 const std::string& observed_at) {
    const bool previous_shutdown_unclean = std::filesystem::exists(path_);
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path());
    }
    const auto temporary = path_.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) throw PrivacyStorageError("cannot open desktop session marker");
    output << "{\"schema_version\":\"1.0\",\"session_id\":\""
           << json_escape(session_id) << "\",\"started_at\":\""
           << json_escape(observed_at) << "\"}\n";
    output.close();
    if (!output) {
        std::filesystem::remove(temporary);
        throw PrivacyStorageError("cannot write desktop session marker");
    }
    replace_file(temporary, path_);
    active_ = true;
    return previous_shutdown_unclean;
}

void DesktopSessionMarker::finish_clean() {
    if (!active_) return;
    std::error_code error;
    std::filesystem::remove(path_, error);
    if (error) {
        throw PrivacyStorageError("cannot remove desktop session marker: " +
                                  error.message());
    }
    active_ = false;
}

bool DesktopSessionMarker::active() const noexcept { return active_; }

const std::filesystem::path& DesktopSessionMarker::path() const noexcept {
    return path_;
}

std::string desktop_session_phase_name(DesktopSessionPhase phase) {
    switch (phase) {
    case DesktopSessionPhase::onboarding: return "onboarding";
    case DesktopSessionPhase::starting: return "starting";
    case DesktopSessionPhase::running: return "running";
    case DesktopSessionPhase::paused: return "paused";
    case DesktopSessionPhase::degraded: return "degraded";
    case DesktopSessionPhase::stopping: return "stopping";
    case DesktopSessionPhase::stopped: return "stopped";
    }
    throw std::invalid_argument("unknown desktop session phase");
}

bool desktop_session_transition_allowed(DesktopSessionPhase from,
                                        DesktopSessionPhase to) {
    if (from == to) return true;
    using Phase = DesktopSessionPhase;
    switch (from) {
    case Phase::stopped:
        return to == Phase::onboarding || to == Phase::starting ||
               to == Phase::degraded;
    case Phase::onboarding:
        return to == Phase::starting || to == Phase::stopping ||
               to == Phase::degraded;
    case Phase::starting:
        return to == Phase::running || to == Phase::paused ||
               to == Phase::degraded || to == Phase::stopping;
    case Phase::running:
        return to == Phase::paused || to == Phase::degraded ||
               to == Phase::stopping;
    case Phase::paused:
        return to == Phase::running || to == Phase::degraded ||
               to == Phase::stopping;
    case Phase::degraded:
        return to == Phase::running || to == Phase::paused ||
               to == Phase::stopping;
    case Phase::stopping:
        return to == Phase::stopped || to == Phase::degraded;
    }
    return false;
}

bool DesktopSessionState::valid() const {
    if (schema_version != "1.0" || session_id.empty() || observed_at.empty()) {
        return false;
    }
    if (paused != (state == DesktopSessionPhase::paused)) return false;
    if ((state == DesktopSessionPhase::onboarding ||
         state == DesktopSessionPhase::stopped || paused) &&
        !active_sensor_ids.empty()) {
        return false;
    }
    auto sorted = active_sensor_ids;
    std::ranges::sort(sorted);
    return std::ranges::adjacent_find(sorted) == sorted.end();
}

std::string DesktopSessionState::to_json() const {
    if (!valid()) throw std::invalid_argument("invalid DesktopSessionState");
    std::ostringstream output;
    output << "{\"schema_version\":\"1.0\",\"session_id\":\""
           << json_escape(session_id) << "\",\"state\":\""
           << desktop_session_phase_name(state) << "\",\"observed_at\":\""
           << json_escape(observed_at) << "\",\"consent_ready\":"
           << (consent_ready ? "true" : "false") << ",\"active_sensor_ids\":[";
    for (std::size_t index = 0; index < active_sensor_ids.size(); ++index) {
        if (index != 0) output << ',';
        output << '"' << json_escape(active_sensor_ids[index]) << '"';
    }
    output << "],\"paused\":" << (paused ? "true" : "false")
           << ",\"previous_shutdown_unclean\":"
           << (previous_shutdown_unclean ? "true" : "false")
           << ",\"model_available\":"
           << (model_available ? "true" : "false") << ",\"reason_code\":";
    if (reason_code) {
        output << '"' << json_escape(*reason_code) << '"';
    } else {
        output << "null";
    }
    output << '}';
    return output.str();
}

bool DesktopPerformanceSample::valid() const {
    if (schema_version != "1.0" || sample_id.empty() || observed_at.empty() ||
        value < 0.0 || sample_count == 0 || environment.empty()) {
        return false;
    }
    if (metric == DesktopPerformanceMetric::idle_cpu) return unit == "percent";
    return unit == "milliseconds";
}

std::string DesktopPerformanceSample::to_json() const {
    if (!valid()) throw std::invalid_argument("invalid DesktopPerformanceSample");
    const auto metric_name = [this]() {
        switch (metric) {
        case DesktopPerformanceMetric::tray_activation: return "tray_activation";
        case DesktopPerformanceMetric::frame_time: return "frame_time";
        case DesktopPerformanceMetric::idle_cpu: return "idle_cpu";
        case DesktopPerformanceMetric::shutdown: return "shutdown";
        }
        return "unknown";
    }();
    const auto statistic_name = [this]() {
        switch (statistic) {
        case DesktopPerformanceStatistic::raw: return "raw";
        case DesktopPerformanceStatistic::mean: return "mean";
        case DesktopPerformanceStatistic::p50: return "p50";
        case DesktopPerformanceStatistic::p95: return "p95";
        case DesktopPerformanceStatistic::p99: return "p99";
        }
        return "unknown";
    }();
    std::ostringstream output;
    output << "{\"schema_version\":\"1.0\",\"sample_id\":\""
           << json_escape(sample_id) << "\",\"observed_at\":\""
           << json_escape(observed_at) << "\",\"metric\":\"" << metric_name
           << "\",\"statistic\":\"" << statistic_name << "\",\"value\":"
           << std::setprecision(15) << value << ",\"unit\":\"" << unit
           << "\",\"sample_count\":" << sample_count
           << ",\"environment\":\"" << json_escape(environment) << "\"}";
    return output.str();
}

std::string desktop_utc_now() {
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    const std::time_t value = std::chrono::system_clock::to_time_t(seconds);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &value);
#else
    gmtime_r(&value, &utc);
#endif
    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string desktop_session_id() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    return "desktop-session-" + std::to_string(ticks);
}

}  // namespace eu_digital
