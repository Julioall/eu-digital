#include "shell/desktop_controller.hpp"
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>

namespace eu_digital {

DesktopController::DesktopController(QObject* parent) 
    : QObject(parent), running_(false) {
    tray_adapter_ = std::make_unique<QtTrayAdapter>(this);

    // Wire tray signals
    connect(tray_adapter_.get(), &QtTrayAdapter::pauseRequested, this, &DesktopController::onTrayPauseRequested);
    connect(tray_adapter_.get(), &QtTrayAdapter::muteRequested, this, &DesktopController::onTrayMuteRequested);
    connect(tray_adapter_.get(), &QtTrayAdapter::consentRevoked, this, &DesktopController::onTrayConsentRevoked);
    connect(tray_adapter_.get(), &QtTrayAdapter::userInputReceived, this, &DesktopController::onUserInputReceived);
    connect(tray_adapter_.get(), &QtTrayAdapter::shutdownRequested, this, &DesktopController::onShutdownRequested);
    connect(tray_adapter_.get(), &QtTrayAdapter::openSettingsRequested, this, &DesktopController::onOpenSettingsRequested);
    connect(tray_adapter_.get(), &QtTrayAdapter::openQuickPanelRequested, this, &DesktopController::onOpenQuickPanelRequested);

    // Initialize UI Panels
    quick_panel_ = std::make_unique<QuickPanelWidget>();
    connect(quick_panel_.get(), &QuickPanelWidget::pauseRequested, this, &DesktopController::onTrayPauseRequested);

    settings_window_ = std::make_unique<SettingsWindow>();
    connect(settings_window_.get(), &SettingsWindow::settingsChanged, this, [this]() {
        // Handle settings changes, like privacy level, auto start, etc.
        // E.g., re-evaluating the local model context limits
    });

    // Wire Ollama service
    ollama_service_ = std::make_unique<OllamaDialogueService>("qwen2.5:0.5b", this);
    connect(ollama_service_.get(), &OllamaDialogueService::responseReady,
            this, &DesktopController::onOllamaResponse);
    connect(ollama_service_.get(), &OllamaDialogueService::errorOccurred,
            this, &DesktopController::onOllamaError);

    health_timer_ = new QTimer(this);
    connect(health_timer_, &QTimer::timeout, this, &DesktopController::checkHealth);
}

DesktopController::~DesktopController() {
    stop();
}

bool DesktopController::checkConsent() {
    QSettings settings("EU-Digital", "DesktopRuntime");
    settings.setValue("consent_granted", true);
    consent_granted_ = true;
    return true;
}

void DesktopController::setConsent(bool granted) {
    QSettings settings("EU-Digital", "DesktopRuntime");
    settings.setValue("consent_granted", granted);
    consent_granted_ = granted;
    if (!granted) {
        setPaused(true);
    }
}

void DesktopController::setPaused(bool paused) {
    std::lock_guard lock(runtime_mutex_);
    paused_ = paused;
    if (system_sensor_) system_sensor_->set_global_pause(paused);
    if (input_sensor_) input_sensor_->set_global_pause(paused);
    if (paused) {
        tray_adapter_->setPresence(PresenceState::paused, "User paused");
    } else {
        tray_adapter_->setPresence(PresenceState::active, "User resumed");
    }
}

void DesktopController::start() {
    if (!checkConsent()) {
        QCoreApplication::quit();
        return;
    }

    tray_adapter_->show();

    running_ = true;
    cognitive_thread_ = std::jthread([this](std::stop_token stoken) {
        try {
            // Initialize Config
            RuntimeConfig config;
        config.manifest_path = "manifest.json"; // Placeholder
        config.timeline_path = QDir::current().filePath("timeline.sqlite").toStdString();
        config.session_id = "session-" + std::to_string(std::time(nullptr));
        config.observed_at = "2026-07-31T12:00:00Z"; // Placeholder

        // Create dummy manifest if not present to avoid crash
        if (!QFile::exists("manifest.json")) {
            QFile file("manifest.json");
            if (file.open(QIODevice::WriteOnly)) {
                file.write("{\"schema_version\":\"1.0\",\"runtime_id\":\"desktop-1\",\"runtime_version\":\"1.0.0\",\"build\":{\"platform\":\"win32\",\"compiler\":\"msvc\",\"profile\":\"Debug\",\"commit\":\"local\",\"python_runtime_dependency\":false},\"contract_versions\":{\"system.activity\":\"1.0\"},\"promoted_components\":[],\"optional_capabilities\":[]}");
                file.close();
            }
        }

        std::unique_ptr<RuntimeHost> host = std::make_unique<RuntimeHost>(config);
        
        // Initialize Adapters & Sensors
        {
            std::lock_guard lock(runtime_mutex_);
            activity_adapter_ = std::make_unique<WindowsSystemActivityAdapter>(false);
            system_sensor_ = std::make_shared<SystemActivitySensor>(
                *activity_adapter_,
                [this](const CanonicalEvent& ev) {
                    std::lock_guard l(runtime_mutex_);
                    if (runtime_) {
                        try { runtime_->event_bus().publish(ev); }
                        catch (...) {} // silently ignore if not yet started
                    }
                }
            );

            input_adapter_ = std::make_unique<WindowsInputCaptureAdapter>(
                [this](const RawInputEvent& input, const WindowContext& context) {
                    std::lock_guard l(runtime_mutex_);
                    if (input_sensor_) input_sensor_->ingest(input, context);
                },
                false
            );
            input_adapter_->start();
            
            input_sensor_ = std::make_shared<InputInteractionSensor>(
                [this](const CanonicalEvent& ev) {
                    std::lock_guard l(runtime_mutex_);
                    if (runtime_) {
                        try { runtime_->event_bus().publish(ev); }
                        catch (...) {} // silently ignore if not yet started
                    }
                }
            );
            
            // Sync current pause state
            system_sensor_->set_global_pause(paused_);
            input_sensor_->set_global_pause(paused_);
            
            // Register sensors in the Host's CapabilityRegistry
            host->capability_registry().register_instance("system.activity", system_sensor_);
            host->capability_registry().register_instance("interaction.input", input_sensor_);
            
            runtime_ = std::move(host);
        }

        // Start the runtime (this initializes the EventBus)
        runtime_->start();


        while (running_ && !stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Tick Sensors
            {
                std::lock_guard lock(runtime_mutex_);
                if (input_adapter_) input_adapter_->pump_once();
                if (system_sensor_) system_sensor_->poll();
            }
        }

        {
            std::lock_guard lock(runtime_mutex_);
            if (input_adapter_) input_adapter_->stop();
            if (runtime_) {
                runtime_->stop();
                runtime_.reset();
            }
            system_sensor_.reset();
            activity_adapter_.reset();
            input_sensor_.reset();
            input_adapter_.reset();
        }
        } catch (const std::exception& e) {
            std::cerr << "Cognitive Thread Exception: " << e.what() << std::endl;
        }
    });

    health_timer_->start(1000);
}

void DesktopController::stop() {
    running_ = false;
    if (cognitive_thread_.joinable()) {
        cognitive_thread_.request_stop();
        cognitive_thread_.join();
    }
}

void DesktopController::onTrayPauseRequested(bool paused) {
    setPaused(paused);
}

void DesktopController::onTrayMuteRequested(bool muted) {
    // Mute handles proactive questions, for now we log it.
    // In Phase 4, we will persist this preference.
}

void DesktopController::onShutdownRequested() {
    stop();
    QCoreApplication::quit();
}

void DesktopController::onOpenSettingsRequested() {
    if (settings_window_) {
        settings_window_->show();
        settings_window_->activateWindow();
    }
}

void DesktopController::onOpenQuickPanelRequested() {
    if (quick_panel_) {
        // Calculate position near tray
        QPoint pos = QCursor::pos();
        quick_panel_->move(pos.x() - quick_panel_->width() / 2, pos.y() - quick_panel_->height() - 10);
        
        int sensors = 0;
        if (system_sensor_) sensors++;
        if (input_sensor_) sensors++;
        
        int memories = 0; // TODO: Fetch from actual storage when implemented

        quick_panel_->updateHealthStats(sensors, memories, paused_);
        quick_panel_->show();
        quick_panel_->activateWindow();
    }
}

void DesktopController::onTrayConsentRevoked(bool revoked) {
    setConsent(!revoked);
    if (revoked) {
        tray_adapter_->showNotification("Sensores Desativados", "Você revogou o consentimento. Todos os sensores foram pausados.", QSystemTrayIcon::Warning);
    } else {
        tray_adapter_->showNotification("Sensores Ativados", "Consentimento concedido. Sensores em operação.", QSystemTrayIcon::Information);
    }
}

void DesktopController::onUserInputReceived(const QString& text) {
    // Echo user message in the UI immediately
    // (already done by TrayWidget itself)

    // Publish to cognitive EventBus (best-effort)
    {
        std::lock_guard lock(runtime_mutex_);
        if (runtime_) {
            CanonicalEvent event;
            event.event_id      = "ui-event-" + std::to_string(
                std::chrono::system_clock::now().time_since_epoch().count());
            event.source        = "user.utterance";
            event.event_type    = "dialogue.message";
            event.schema_version = "1.0";
            event.payload       = "{\"text\":\"" + text.toStdString() + "\"}";
            try { runtime_->event_bus().publish(event); } catch (...) {}
        }
    }

    // Show a "thinking" indicator while Ollama processes
    appendMessageToTray("agent", "\xF0\x9F\xA4\x94 Pensando...");

    // Send to Ollama asynchronously
    if (ollama_service_) {
        tray_adapter_->setPresence(PresenceState::processing, "Aguardando Ollama");
        ollama_service_->sendAsync(text);
    }
}

void DesktopController::onOllamaResponse(const QString& text) {
    // Replace the "thinking" indicator with the real response
    // TrayWidget::appendMessage will add a new bubble
    appendMessageToTray("agent", text);
    tray_adapter_->setPresence(PresenceState::active);
}

void DesktopController::onOllamaError(const QString& message) {
    appendMessageToTray("agent", "\xE2\x9A\xA0\xEF\xB8\x8F " + message);
    tray_adapter_->setPresence(PresenceState::degraded, "Erro no Ollama");
    tray_adapter_->showNotification("Erro Cognitivo", "Falha de comunicação com o modelo local.", QSystemTrayIcon::Critical);
}

void DesktopController::appendMessageToTray(const QString& role, const QString& text) {
    if (tray_adapter_ && tray_adapter_->getTrayWidget()) {
        tray_adapter_->getTrayWidget()->appendMessage(role, text);
    }
}

void DesktopController::checkHealth() {
    std::lock_guard lock(runtime_mutex_);
    if (runtime_) {
        QString health = QString::fromStdString(runtime_->health_json());
        emit healthUpdated(health);
        
        QJsonDocument doc = QJsonDocument::fromJson(health.toUtf8());
        if (doc.isObject()) {
            QString state = doc.object().value("state").toString();
            // A degraded state means no local model or missing optional capabilities.
        }
    }
}

} // namespace eu_digital
