#pragma once

#include "core/runtime_host.hpp"
#include "core/system_activity_sensor.hpp"
#include "core/input_interaction_sensor.hpp"
#include "shell/qt_tray_adapter.hpp"
#include "shell/qt_avatar_window.hpp"

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

private slots:
    void onTrayPauseRequested(bool paused);
    void onTrayConsentRevoked(bool revoked);
    void checkHealth();

private:
    void cognitiveThreadMain();

    bool consent_granted_{false};
    bool paused_{false};
    std::atomic<bool> running_{false};
    std::jthread cognitive_thread_;
    std::mutex runtime_mutex_;

    // Runtime Core
    std::unique_ptr<RuntimeHost> runtime_;
    
    // Sensors
    std::unique_ptr<SystemActivitySensor> system_sensor_;
    std::unique_ptr<InputInteractionSensor> input_sensor_;

    // UI Adapters
    std::unique_ptr<QtTrayAdapter> tray_adapter_;
    QTimer* health_timer_{nullptr};
};

} // namespace eu_digital
