#include "shell/desktop_runtime_lifecycle.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace eu_digital;

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() /
        ("eu-digital-desktop-lifecycle-" + desktop_session_id());
    fs::create_directories(root);

    try {
        const auto consent_path = root / "consent.dpapi";
        DesktopConsentStore consent(consent_path);
        assert(consent.load());
        assert(!consent.any_granted());
        assert(!consent.all_granted());

        consent.grant(kSystemActivitySensorId, kSystemActivityPurpose,
                      "2026-08-04T12:00:00Z");
        assert(consent.capture_allowed(kSystemActivitySensorId,
                                       kSystemActivityPurpose));
        assert(!consent.capture_allowed(kInputInteractionSensorId,
                                        kInputInteractionPurpose));
        assert(consent.any_granted());
        assert(!consent.all_granted());

        consent.grant(kInputInteractionSensorId, kInputInteractionPurpose,
                      "2026-08-04T12:01:00Z");
        assert(consent.all_granted());
        consent.set_paused(true);
        assert(!consent.any_granted());
        consent.set_paused(false);
        assert(consent.all_granted());
        consent.revoke(kSystemActivitySensorId, kSystemActivityPurpose,
                       "2026-08-04T12:02:00Z");
        assert(!consent.capture_allowed(kSystemActivitySensorId,
                                        kSystemActivityPurpose));
        assert(consent.capture_allowed(kInputInteractionSensorId,
                                       kInputInteractionPurpose));

        DesktopConsentStore restored(consent_path);
        assert(restored.load());
        assert(!restored.capture_allowed(kSystemActivitySensorId,
                                         kSystemActivityPurpose));
        assert(restored.capture_allowed(kInputInteractionSensorId,
                                        kInputInteractionPurpose));

        {
            std::ofstream corrupt(consent_path, std::ios::binary | std::ios::trunc);
            corrupt << "not a DPAPI ledger";
        }
        DesktopConsentStore denied(consent_path);
        assert(!denied.load());
        assert(!denied.loaded_cleanly());
        assert(denied.error_code() == "consent_ledger_unreadable");
        assert(!denied.any_granted());

        const auto marker_path = root / "session.marker";
        DesktopSessionMarker first(marker_path);
        assert(!first.begin("session-1", "2026-08-04T12:00:00Z"));
        assert(first.active());
        DesktopSessionMarker recovery(marker_path);
        assert(recovery.begin("session-2", "2026-08-04T12:01:00Z"));
        recovery.finish_clean();
        assert(!fs::exists(marker_path));

        assert(desktop_session_transition_allowed(DesktopSessionPhase::stopped,
                                                  DesktopSessionPhase::onboarding));
        assert(desktop_session_transition_allowed(DesktopSessionPhase::starting,
                                                  DesktopSessionPhase::degraded));
        assert(!desktop_session_transition_allowed(DesktopSessionPhase::running,
                                                   DesktopSessionPhase::onboarding));

        DesktopSessionState state;
        state.session_id = "session-2";
        state.state = DesktopSessionPhase::degraded;
        state.observed_at = "2026-08-04T12:01:00Z";
        state.consent_ready = false;
        state.active_sensor_ids = {kInputInteractionSensorId};
        state.previous_shutdown_unclean = true;
        state.reason_code = "previous_shutdown_unclean";
        assert(state.valid());
        const auto json = state.to_json();
        assert(json.find("\"schema_version\":\"1.0\"") != std::string::npos);
        assert(json.find("\"previous_shutdown_unclean\":true") !=
               std::string::npos);

        state.state = DesktopSessionPhase::paused;
        state.paused = true;
        assert(!state.valid());
        state.active_sensor_ids.clear();
        assert(state.valid());

        DesktopPerformanceSample performance;
        performance.sample_id = "sample-1";
        performance.observed_at = "2026-08-04T12:03:00Z";
        performance.metric = DesktopPerformanceMetric::idle_cpu;
        performance.statistic = DesktopPerformanceStatistic::mean;
        performance.value = 0.5;
        performance.unit = "percent";
        performance.sample_count = 100;
        performance.environment = "windows-qt-offscreen";
        assert(performance.valid());
        assert(performance.to_json().find("\"metric\":\"idle_cpu\"") !=
               std::string::npos);
        performance.unit = "milliseconds";
        assert(!performance.valid());

        fs::remove_all(root);
        std::cout << "Desktop runtime lifecycle tests passed.\n";
        return 0;
    } catch (...) {
        fs::remove_all(root);
        throw;
    }
}
