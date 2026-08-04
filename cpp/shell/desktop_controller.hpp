#pragma once

#include "core/runtime_host.hpp"
#include "core/system_activity_sensor.hpp"
#include "core/input_interaction_sensor.hpp"
#include "core/adapters/cognitive_port_factory.hpp"
#include "core/episodic_memory.hpp"
#include "core/world_model.hpp"
#include "core/global_workspace.hpp"
#include "core/functional_self_model.hpp"
#include "core/metacognition_curiosity.hpp"
#include "core/suggestion_orchestrator.hpp"
#include "core/adapters/local_language_renderer.hpp"
#include "shell/desktop_runtime_lifecycle.hpp"
#include "shell/qt_tray_adapter.hpp"
#include "shell/qt_dialogue_presentation_adapter.hpp"
#include "shell/qt_avatar_window.hpp"
#include "shell/settings_window.hpp"

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QTimer>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <string>
#include <vector>

namespace eu_digital {

struct DesktopControllerConfig {
    std::filesystem::path data_directory;
    std::filesystem::path manifest_path;
    bool show_onboarding{true};
    bool enable_real_sensors{true};
};

class DesktopController : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit DesktopController(QObject* parent = nullptr);
    explicit DesktopController(DesktopControllerConfig config,
                               QObject* parent = nullptr);
    ~DesktopController() override;

    void start();
    void stop();

    bool checkConsent();
    void setConsent(bool granted);
    void setSensorConsent(const std::string& sensor_id,
                          const std::string& purpose, bool granted);

    void setPaused(bool paused);
    std::string sessionStateJson() const;
    std::uint64_t sensorEventCount() const noexcept;
    bool nativeEventFilter(const QByteArray& event_type, void* message,
                           qintptr* result) override;

signals:
    void healthUpdated(const QString& health_json);
    void sessionStateUpdated(const QString& state_json);
    void consentChanged(bool granted);
    void cognitiveCycleResultReceived(const QString& payload);

private slots:
    void onTrayPauseRequested(bool paused);
    void onTrayMuteRequested(bool muted);
    void onTrayConsentRevoked(bool revoked);
    void onShutdownRequested();
    void onOpenSettingsRequested();
    void checkHealth();
    void onUserInputReceived(const QString& text);
    void onCognitiveCycleResultReceived(const QString& payload);

public slots:
    void appendMessageToTray(const QString& role, const QString& text);

private:
    static DesktopControllerConfig defaultConfig();
    void cleanupRuntimeComponents();
    void transitionSession(DesktopSessionPhase next,
                           std::vector<std::string> active_sensor_ids = {},
                           std::optional<std::string> reason_code = std::nullopt);
    bool sensorAllowed(const std::string& sensor_id,
                       const std::string& purpose) const;
    void setSensorCapabilityAvailable(const std::string& implementation_id,
                                      bool available,
                                      const std::string& reason_code);

    bool consent_granted_{false};
    std::atomic<bool> paused_{false};
    bool previous_shutdown_unclean_{false};
    std::atomic<bool> model_available_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};
    std::atomic<std::uint64_t> sensor_event_count_{0};
    std::jthread cognitive_thread_;
    std::recursive_mutex runtime_mutex_;
    mutable std::mutex session_mutex_;
    DesktopControllerConfig config_;
    std::string session_id_;
    DesktopSessionState session_state_;
    std::unique_ptr<DesktopConsentStore> consent_store_;
    std::unique_ptr<DesktopSessionMarker> session_marker_;

    // Runtime Core
    std::unique_ptr<RuntimeHost> runtime_;
    
    // Sensors
    std::unique_ptr<SystemActivityAdapter> activity_adapter_;
    std::shared_ptr<SystemActivitySensor> system_sensor_;
    std::unique_ptr<WindowsInputCaptureAdapter> input_adapter_;
    std::shared_ptr<InputInteractionSensor> input_sensor_;

    // UI Adapters
    std::unique_ptr<QtTrayAdapter> tray_adapter_;
    std::unique_ptr<SettingsWindow> settings_window_;
    QTimer* health_timer_{nullptr};

    // Ollama Dialogue (Removed in SPEC-053)

    // Cognitive Components (Fase 2 — SPEC-053)
    std::shared_ptr<EpisodicMemoryStore> episodic_memory_;
    std::shared_ptr<PatternLearner> pattern_learner_;
    std::shared_ptr<WorldModel> world_model_;
    std::shared_ptr<GlobalWorkspace> global_workspace_;
    std::shared_ptr<VersionedFunctionalSelfModel> self_model_;
    std::shared_ptr<MetacognitionCuriosityEngine> metacognition_engine_;
    std::shared_ptr<SuggestionOrchestrator> suggestion_orchestrator_;
};

} // namespace eu_digital
