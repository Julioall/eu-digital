#pragma once

#include "core/privacy_storage.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace eu_digital {

inline constexpr char kSystemActivitySensorId[] = "system_activity";
inline constexpr char kSystemActivityPurpose[] = "local_activity_observation";
inline constexpr char kInputInteractionSensorId[] = "input_interaction";
inline constexpr char kInputInteractionPurpose[] = "local_interaction_observation";
inline constexpr char kDesktopConsentVersion[] = "1.0";

struct DesktopConsentPair {
    std::string sensor_id;
    std::string purpose;
};

const std::vector<DesktopConsentPair>& desktop_sensor_consents();

class DesktopConsentStore {
public:
    explicit DesktopConsentStore(std::filesystem::path path);

    bool load();
    bool loaded_cleanly() const noexcept;
    const std::string& error_code() const noexcept;
    bool capture_allowed(const std::string& sensor_id,
                         const std::string& purpose) const;
    bool any_granted() const;
    bool all_granted() const;
    bool paused() const noexcept;

    void grant(const std::string& sensor_id, const std::string& purpose,
               const std::string& decided_at);
    void revoke(const std::string& sensor_id, const std::string& purpose,
                const std::string& decided_at);
    void grant_all(const std::string& decided_at);
    void revoke_all(const std::string& decided_at);
    void set_paused(bool paused);

    const ConsentLedger& ledger() const noexcept;

private:
    void persist();

    std::filesystem::path path_;
    ConsentLedger ledger_;
    bool loaded_cleanly_{true};
    std::string error_code_;
};

class DesktopSessionMarker {
public:
    explicit DesktopSessionMarker(std::filesystem::path path);

    bool begin(const std::string& session_id, const std::string& observed_at);
    void finish_clean();
    bool active() const noexcept;
    const std::filesystem::path& path() const noexcept;

private:
    std::filesystem::path path_;
    bool active_{false};
};

enum class DesktopSessionPhase {
    onboarding,
    starting,
    running,
    paused,
    degraded,
    stopping,
    stopped,
};

std::string desktop_session_phase_name(DesktopSessionPhase phase);
bool desktop_session_transition_allowed(DesktopSessionPhase from,
                                        DesktopSessionPhase to);

struct DesktopSessionState {
    std::string schema_version{"1.0"};
    std::string session_id;
    DesktopSessionPhase state{DesktopSessionPhase::stopped};
    std::string observed_at;
    bool consent_ready{false};
    std::vector<std::string> active_sensor_ids;
    bool paused{false};
    bool previous_shutdown_unclean{false};
    bool model_available{false};
    std::optional<std::string> reason_code;

    bool valid() const;
    std::string to_json() const;
};

enum class DesktopPerformanceMetric {
    tray_activation,
    frame_time,
    idle_cpu,
    shutdown,
};

enum class DesktopPerformanceStatistic { raw, mean, p50, p95, p99 };

struct DesktopPerformanceSample {
    std::string schema_version{"1.0"};
    std::string sample_id;
    std::string observed_at;
    DesktopPerformanceMetric metric{DesktopPerformanceMetric::tray_activation};
    DesktopPerformanceStatistic statistic{DesktopPerformanceStatistic::raw};
    double value{0.0};
    std::string unit{"milliseconds"};
    std::uint64_t sample_count{1};
    std::string environment;

    bool valid() const;
    std::string to_json() const;
};

std::string desktop_utc_now();
std::string desktop_session_id();

}  // namespace eu_digital
