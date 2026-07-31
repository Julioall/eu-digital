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

    // Initialize UI Panels
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
    consent_granted_ = settings.value("consent_granted", false).toBool();
    return consent_granted_;
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
        // SPEC-030: consent is deny-by-default. Show onboarding dialog.
        auto answer = QMessageBox::question(
            nullptr,
            "Eu Digital — Consentimento de Observação",
            "O Eu Digital precisa do seu consentimento para observar atividades "
            "do sistema (janelas ativas, padrões de digitação) de forma local e privada.\n\n"
            "Nenhum dado será enviado para a nuvem.\n\n"
            "Você autoriza a observação local?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (answer == QMessageBox::Yes) {
            setConsent(true);
        } else {
            setConsent(false);
            // Show tray in passive mode without sensors
            tray_adapter_->show();
            tray_adapter_->setPresence(PresenceState::paused, "Aguardando consentimento");
            health_timer_->start(1000);
            return;
        }
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
            
            // Instantiate cognitive components and register ports (SPEC-053 Fase 2)
            episodic_memory_ = std::make_shared<EpisodicMemoryStore>();
            
            WorldModelConfig wm_config;
            world_model_ = std::make_shared<WorldModel>(wm_config, "desktop");
            
            global_workspace_ = std::make_shared<GlobalWorkspace>(
                "gw-desktop", config.session_id, WorkspaceConfig{});
            
            self_model_ = std::make_shared<VersionedFunctionalSelfModel>(
                "eu-digital-desktop", config.observed_at);
            
            metacognition_engine_ = std::make_shared<MetacognitionCuriosityEngine>();
            suggestion_orchestrator_ = std::make_shared<SuggestionOrchestrator>();
            
            host->capability_registry().register_instance("episode_boundary",
                CognitivePortFactory::create_episode_boundary_port());
            host->capability_registry().register_instance("memory_write",
                CognitivePortFactory::create_memory_write_port(episodic_memory_));
            host->capability_registry().register_instance("prediction",
                CognitivePortFactory::create_prediction_port(world_model_));
            host->capability_registry().register_instance("workspace",
                CognitivePortFactory::create_workspace_selection_port(global_workspace_));
            host->capability_registry().register_instance("self_model",
                CognitivePortFactory::create_self_model_query_port(self_model_));
            host->capability_registry().register_instance("metacognition",
                CognitivePortFactory::create_metacognition_port(metacognition_engine_));
            host->capability_registry().register_instance("decision",
                CognitivePortFactory::create_cognitive_decision_port(suggestion_orchestrator_));
            
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
        std::lock_guard lock(runtime_mutex_);
        if (runtime_) {
            settings_window_->setCapabilityRegistry(&runtime_->capability_registry());
        }
        settings_window_->show();
        settings_window_->activateWindow();
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
            QJsonObject payload_obj;
            payload_obj["text"] = text;
            QJsonDocument payload_doc(payload_obj);
            event.payload = payload_doc.toJson(QJsonDocument::Compact).toStdString();
            try { runtime_->event_bus().publish(event); } catch (...) {}
        }
    }

    // Show a "thinking" indicator while the cognitive cycle processes
    appendMessageToTray("agent", "\xF0\x9F\xA4\x94 Pensando...");
    tray_adapter_->setPresence(PresenceState::processing, "Processando ciclo cognitivo");

    // Wait briefly for the cognitive cycle to process, then send enriched prompt to Ollama.
    // The cycle runs on a background thread; we use a short delay to let it complete.
    QTimer::singleShot(200, this, [this, text]() {
        if (!ollama_service_) return;

        // Build enriched prompt from cognitive context
        QString enriched_prompt = text;
        {
            std::lock_guard lock(runtime_mutex_);
            if (runtime_) {
                auto coordinator = runtime_->coordinator();
                if (coordinator) {
                    auto ctx = coordinator->last_cycle_context();

                    // Enrich the prompt with cognitive context
                    QStringList context_parts;

                    if (ctx.episode && ctx.episode->valid()) {
                        context_parts << QString("Episódio atual: %1 (%2)")
                            .arg(QString::fromStdString(ctx.episode->episode_id))
                            .arg(QString::fromStdString(ctx.episode->current_state));
                    }

                    if (ctx.self_model && ctx.self_model->valid()) {
                        context_parts << QString("Modelo: %1, alinhamento: %2")
                            .arg(QString::fromStdString(ctx.self_model->model_id))
                            .arg(ctx.self_model->alignment_score);
                    }

                    if (ctx.decision) {
                        context_parts << QString("Decisão cognitiva: intent=%1, razão=%2")
                            .arg(QString::fromStdString(ctx.decision->intent))
                            .arg(QString::fromStdString(ctx.decision->reason));
                    }

                    if (ctx.metacognition && ctx.metacognition->valid()) {
                        context_parts << QString("Curiosidade: %1, explorar: %2")
                            .arg(ctx.metacognition->curiosity_score)
                            .arg(ctx.metacognition->requires_exploration ? "sim" : "não");
                    }

                    if (!context_parts.isEmpty()) {
                        enriched_prompt = QString("[Contexto cognitivo: %1]\n\nUsuário: %2")
                            .arg(context_parts.join("; "))
                            .arg(text);
                    }
                }
            }
        }

        tray_adapter_->setPresence(PresenceState::processing, "Aguardando Ollama");
        ollama_service_->sendAsync(enriched_prompt);
    });
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
