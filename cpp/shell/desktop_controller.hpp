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
#include "shell/qt_tray_adapter.hpp"
#include "shell/qt_avatar_window.hpp"
#include "shell/settings_window.hpp"

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace eu_digital {

class DesktopController : public QObject {
    Q_OBJECT
public:
    explicit DesktopController(QObject* parent = nullptr);
    ~DesktopController() override;

    void start();
    void stop();

    bool checkConsent();
    void setConsent(bool granted);

    void setPaused(bool paused);

signals:
    void healthUpdated(const QString& health_json);
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
    void cognitiveThreadMain();

    bool consent_granted_{false};
    bool paused_{false};
    std::atomic<bool> running_{false};
    std::jthread cognitive_thread_;
    std::recursive_mutex runtime_mutex_;

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
    std::shared_ptr<WorldModel> world_model_;
    std::shared_ptr<GlobalWorkspace> global_workspace_;
    std::shared_ptr<VersionedFunctionalSelfModel> self_model_;
    std::shared_ptr<MetacognitionCuriosityEngine> metacognition_engine_;
    std::shared_ptr<SuggestionOrchestrator> suggestion_orchestrator_;
};

} // namespace eu_digital
