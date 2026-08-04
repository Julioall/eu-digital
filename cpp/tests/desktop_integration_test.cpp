#include "shell/desktop_controller.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <cassert>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace eu_digital;

namespace {

std::atomic<int> rejected_transition_diagnostics{0};

void diagnostic_counter(QtMsgType, const QMessageLogContext&,
                        const QString& message) {
    if (message.contains("REJECTED presence transition")) {
        ++rejected_transition_diagnostics;
    }
}

void wait_for_events(int milliseconds) {
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

QJsonObject state_object(const DesktopController& controller) {
    const auto document = QJsonDocument::fromJson(
        QByteArray::fromStdString(controller.sessionStateJson()));
    assert(document.isObject());
    return document.object();
}

DesktopControllerConfig config_for(const std::filesystem::path& root,
                                   const std::filesystem::path& manifest) {
    DesktopControllerConfig config;
    config.data_directory = root;
    config.manifest_path = manifest;
    config.show_onboarding = false;
    config.enable_real_sensors = false;
    return config;
}

double percentile(std::vector<double> values, double fraction) {
    assert(!values.empty());
    std::ranges::sort(values);
    const auto rank = static_cast<std::size_t>(
        std::ceil(fraction * static_cast<double>(values.size())));
    return values[std::clamp<std::size_t>(rank, 1, values.size()) - 1];
}

double process_cpu_seconds() {
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        return 0.0;
    }
    ULARGE_INTEGER kernel_ticks{};
    kernel_ticks.LowPart = kernel.dwLowDateTime;
    kernel_ticks.HighPart = kernel.dwHighDateTime;
    ULARGE_INTEGER user_ticks{};
    user_ticks.LowPart = user.dwLowDateTime;
    user_ticks.HighPart = user.dwHighDateTime;
    return static_cast<double>(kernel_ticks.QuadPart + user_ticks.QuadPart) /
           10'000'000.0;
#else
    return 0.0;
#endif
}

unsigned logical_processor_count() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return std::max<DWORD>(info.dwNumberOfProcessors, 1);
#else
    return 1;
#endif
}

DesktopPerformanceSample performance_sample(
    std::string id, DesktopPerformanceMetric metric,
    DesktopPerformanceStatistic statistic, double value, std::string unit,
    std::uint64_t sample_count) {
    DesktopPerformanceSample sample;
    sample.sample_id = std::move(id);
    sample.observed_at = desktop_utc_now();
    sample.metric = metric;
    sample.statistic = statistic;
    sample.value = value;
    sample.unit = std::move(unit);
    sample.sample_count = sample_count;
    sample.environment = "Windows Qt 6 offscreen Debug; local process measurement";
    assert(sample.valid());
    return sample;
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    QApplication app(argc, argv);
    app.setApplicationName("EU-Digital Desktop Integration Test");
    app.setOrganizationName("EU-Digital Tests");
    app.setQuitOnLastWindowClosed(false);

    const auto executable_directory = fs::path(
        QCoreApplication::applicationDirPath().toStdWString());
    const auto manifest = executable_directory / "runtime_manifest.json";
    if (!fs::is_regular_file(manifest)) {
        std::cerr << "Generated desktop runtime manifest is missing.\n";
        return 1;
    }

    const auto root = fs::temp_directory_path() /
        ("eu-digital-desktop-integration-" + desktop_session_id());
    fs::create_directories(root);

    try {
        {
            const auto data = root / "first-run";
            DesktopController controller(config_for(data, manifest));
            QElapsedTimer start_timer;
            start_timer.start();
            controller.start();
            assert(start_timer.elapsed() < 500);
            const auto state = state_object(controller);
            assert(state.value("state").toString() == "onboarding");
            assert(!state.value("consent_ready").toBool());
            assert(state.value("active_sensor_ids").toArray().isEmpty());
            assert(controller.sensorEventCount() == 0);
            assert(fs::exists(data / "desktop.session.marker"));
            controller.stop();
            assert(!fs::exists(data / "desktop.session.marker"));
            assert(state_object(controller).value("state").toString() == "stopped");
        }

        {
            const auto data = root / "partial-grant";
            DesktopConsentStore consent(data / "consent.dpapi");
            assert(consent.load());
            consent.grant(kSystemActivitySensorId, kSystemActivityPurpose,
                          desktop_utc_now());

            DesktopController controller(config_for(data, manifest));
            bool health_emitted = false;
            QObject::connect(&controller, &DesktopController::healthUpdated,
                             [&](const QString&) { health_emitted = true; });
            controller.start();
            wait_for_events(1200);
            const auto state = state_object(controller);
            assert(state.value("state").toString() == "degraded");
            assert(state.value("reason_code").toString() == "model_unavailable");
            assert(!state.value("consent_ready").toBool());
            assert(!state.value("model_available").toBool());
            assert(health_emitted);
            assert(fs::exists(data / "timeline.sqlite"));
#ifdef _WIN32
            MSG power_message{};
            power_message.message = WM_POWERBROADCAST;
            power_message.wParam = PBT_APMSUSPEND;
            controller.nativeEventFilter({}, &power_message, nullptr);
            assert(state_object(controller).value("reason_code").toString() ==
                   "system_suspended");
            power_message.wParam = PBT_APMRESUMEAUTOMATIC;
            controller.nativeEventFilter({}, &power_message, nullptr);
            assert(state_object(controller).value("reason_code").toString() ==
                   "model_unavailable");
#endif
            controller.stop();
            assert(!fs::exists(data / "desktop.session.marker"));
        }

        {
            const auto data = root / "recovery";
            DesktopConsentStore consent(data / "consent.dpapi");
            assert(consent.load());
            consent.grant_all(desktop_utc_now());
            DesktopSessionMarker orphan(data / "desktop.session.marker");
            assert(!orphan.begin("orphan-session", desktop_utc_now()));

            DesktopController controller(config_for(data, manifest));
            controller.start();
            wait_for_events(250);
            const auto state = state_object(controller);
            assert(state.value("state").toString() == "degraded");
            assert(state.value("previous_shutdown_unclean").toBool());
            assert(state.value("reason_code").toString() ==
                   "previous_shutdown_unclean");
            controller.stop();
            assert(!fs::exists(data / "desktop.session.marker"));
        }

        {
            const auto data = root / "stress";
            DesktopConsentStore consent(data / "consent.dpapi");
            assert(consent.load());
            consent.grant_all(desktop_utc_now());
            DesktopController controller(config_for(data, manifest));
            rejected_transition_diagnostics = 0;
            const auto previous_handler = qInstallMessageHandler(diagnostic_counter);
            for (int iteration = 0; iteration < 5; ++iteration) {
                controller.start();
                controller.setPaused(true);
                controller.setPaused(false);
                QElapsedTimer shutdown_timer;
                shutdown_timer.start();
                controller.stop();
                if (shutdown_timer.elapsed() >= 2000) {
                    std::cerr << "Desktop shutdown watchdog exceeded at iteration "
                              << iteration << ".\n";
                    return 1;
                }
                assert(!fs::exists(data / "desktop.session.marker"));
            }
            qInstallMessageHandler(previous_handler);
            assert(rejected_transition_diagnostics.load() == 0);
        }

        {
            const auto data = root / "performance";
            DesktopConsentStore consent(data / "consent.dpapi");
            assert(consent.load());
            consent.grant_all(desktop_utc_now());
            DesktopController controller(config_for(data, manifest));
            controller.start();
            wait_for_events(500);

            auto* tray = controller.findChild<QtTrayAdapter*>();
            assert(tray != nullptr);
            std::vector<double> tray_samples;
            std::vector<double> frame_samples;
            tray_samples.reserve(200);
            frame_samples.reserve(200);
            for (int index = 0; index < 200; ++index) {
                const auto started = std::chrono::steady_clock::now();
                tray->activateAt(QPoint(100, 100));
                QCoreApplication::processEvents(QEventLoop::AllEvents);
                tray_samples.push_back(std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count());
            }
            tray->getTrayWidget()->show();
            for (int index = 0; index < 200; ++index) {
                const auto started = std::chrono::steady_clock::now();
                tray->getTrayWidget()->update();
                QCoreApplication::processEvents(QEventLoop::AllEvents);
                frame_samples.push_back(std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count());
            }

            const auto cpu_started = process_cpu_seconds();
            QElapsedTimer idle_timer;
            idle_timer.start();
            wait_for_events(2000);
            const auto cpu_finished = process_cpu_seconds();
            const double idle_cpu_percent =
                (cpu_finished - cpu_started) /
                (static_cast<double>(idle_timer.elapsed()) / 1000.0) /
                static_cast<double>(logical_processor_count()) * 100.0;

            QElapsedTimer shutdown_timer;
            shutdown_timer.start();
            controller.stop();
            const double shutdown_ms = static_cast<double>(shutdown_timer.elapsed());

            const double tray_p99 = percentile(tray_samples, 0.99);
            const double frame_p95 = percentile(frame_samples, 0.95);
            const double frame_p99 = percentile(frame_samples, 0.99);
            assert(tray_p99 < 50.0);
            assert(frame_p95 < 16.6);
            assert(frame_p99 < 33.0);
            assert(idle_cpu_percent < 1.0);
            assert(shutdown_ms < 2000.0);

            const std::vector<DesktopPerformanceSample> samples{
                performance_sample("tray-p99", DesktopPerformanceMetric::tray_activation,
                    DesktopPerformanceStatistic::p99, tray_p99, "milliseconds",
                    tray_samples.size()),
                performance_sample("frame-p95", DesktopPerformanceMetric::frame_time,
                    DesktopPerformanceStatistic::p95, frame_p95, "milliseconds",
                    frame_samples.size()),
                performance_sample("frame-p99", DesktopPerformanceMetric::frame_time,
                    DesktopPerformanceStatistic::p99, frame_p99, "milliseconds",
                    frame_samples.size()),
                performance_sample("idle-mean", DesktopPerformanceMetric::idle_cpu,
                    DesktopPerformanceStatistic::mean, idle_cpu_percent, "percent", 1),
                performance_sample("shutdown-raw", DesktopPerformanceMetric::shutdown,
                    DesktopPerformanceStatistic::raw, shutdown_ms, "milliseconds", 1),
            };
            std::ofstream output(executable_directory /
                                     "desktop_performance_samples.jsonl",
                                 std::ios::binary | std::ios::trunc);
            assert(output);
            for (const auto& sample : samples) output << sample.to_json() << '\n';
            output.close();
            assert(output);
        }

        fs::remove_all(root);
        std::cout << "Desktop integration test passed.\n";
        return 0;
    } catch (...) {
        fs::remove_all(root);
        throw;
    }
}
