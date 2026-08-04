#include "shell/desktop_controller.hpp"
#include <QMessageBox>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QDebug>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace eu_digital {
namespace {

QString structuredError(const char* event, const std::exception& error,
                        const char* sensor_id = nullptr) {
    QJsonObject payload;
    payload["event"] = event;
    payload["error"] = QString::fromUtf8(error.what());
    if (sensor_id != nullptr) payload["sensor_id"] = sensor_id;
    return QString::fromUtf8(
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

}  // namespace

DesktopControllerConfig DesktopController::defaultConfig() {
    const auto data_path = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    const auto application_path = QCoreApplication::applicationDirPath();
    DesktopControllerConfig config;
    config.data_directory = std::filesystem::path(data_path.toStdWString());
    config.manifest_path = std::filesystem::path(application_path.toStdWString()) /
                           L"runtime_manifest.json";
    return config;
}

DesktopController::DesktopController(QObject* parent)
    : DesktopController(defaultConfig(), parent) {}

DesktopController::DesktopController(DesktopControllerConfig config,
                                     QObject* parent)
    : QObject(parent), config_(std::move(config)), session_id_(desktop_session_id()) {
    if (config_.data_directory.empty()) {
        config_.data_directory = defaultConfig().data_directory;
    }
    if (config_.manifest_path.empty()) {
        config_.manifest_path = defaultConfig().manifest_path;
    }
    consent_store_ = std::make_unique<DesktopConsentStore>(
        config_.data_directory / "consent.dpapi");
    session_marker_ = std::make_unique<DesktopSessionMarker>(
        config_.data_directory / "desktop.session.marker");
    session_state_.session_id = session_id_;
    session_state_.observed_at = desktop_utc_now();
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
    connect(settings_window_.get(), &SettingsWindow::sensorStateChangeRequested, this, [this](const QString& cap_id, bool pause) {
        if (cap_id == "system.activity") {
            setSensorConsent(kSystemActivitySensorId, kSystemActivityPurpose,
                             !pause);
        } else if (cap_id == "interaction.input") {
            setSensorConsent(kInputInteractionSensorId,
                             kInputInteractionPurpose, !pause);
        }
    });

    health_timer_ = new QTimer(this);
    connect(health_timer_, &QTimer::timeout, this, &DesktopController::checkHealth);
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
}

DesktopController::~DesktopController() {
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
    stop();
}

bool DesktopController::checkConsent() {
    std::lock_guard lock(runtime_mutex_);
    consent_granted_ = consent_store_ && consent_store_->all_granted();
    return consent_granted_;
}

void DesktopController::setConsent(bool granted) {
    const auto decided_at = desktop_utc_now();
    try {
        std::lock_guard lock(runtime_mutex_);
        if (granted) {
            consent_store_->grant_all(decided_at);
            if (system_sensor_) system_sensor_->set_global_pause(false);
            if (input_sensor_) input_sensor_->set_global_pause(false);
            if (input_adapter_) input_adapter_->start();
            setSensorCapabilityAvailable("system.activity_impl", true,
                                         "consent_granted");
            setSensorCapabilityAvailable("interaction.input_impl", true,
                                         "consent_granted");
        } else {
            if (system_sensor_) system_sensor_->set_global_pause(true);
            if (input_sensor_) input_sensor_->set_global_pause(true);
            if (input_adapter_) input_adapter_->stop();
            consent_store_->revoke_all(decided_at);
            setSensorCapabilityAvailable("system.activity_impl", false,
                                         "consent_revoked");
            setSensorCapabilityAvailable("interaction.input_impl", false,
                                         "consent_revoked");
        }
        consent_granted_ = consent_store_->all_granted();
    } catch (const std::exception& error) {
        qCritical().noquote()
            << structuredError("desktop_consent_write_failed", error);
        transitionSession(DesktopSessionPhase::degraded, {},
                          "consent_persistence_failed");
    }
}

void DesktopController::setSensorConsent(const std::string& sensor_id,
                                         const std::string& purpose,
                                         bool granted) {
    const bool known_pair =
        (sensor_id == kSystemActivitySensorId &&
         purpose == kSystemActivityPurpose) ||
        (sensor_id == kInputInteractionSensorId &&
         purpose == kInputInteractionPurpose);
    if (!known_pair) {
        throw std::invalid_argument("unknown desktop sensor consent pair");
    }
    const auto decided_at = desktop_utc_now();
    try {
        std::lock_guard lock(runtime_mutex_);
        if (!granted) {
            if (sensor_id == kSystemActivitySensorId && system_sensor_) {
                system_sensor_->set_global_pause(true);
            }
            if (sensor_id == kInputInteractionSensorId) {
                if (input_sensor_) input_sensor_->set_global_pause(true);
                if (input_adapter_) input_adapter_->stop();
            }
            consent_store_->revoke(sensor_id, purpose, decided_at);
            setSensorCapabilityAvailable(
                sensor_id == kSystemActivitySensorId
                    ? "system.activity_impl" : "interaction.input_impl",
                false, "consent_revoked");
        } else {
            consent_store_->grant(sensor_id, purpose, decided_at);
            if (sensor_id == kSystemActivitySensorId && system_sensor_) {
                system_sensor_->set_global_pause(false);
            }
            if (sensor_id == kInputInteractionSensorId) {
                if (input_sensor_) input_sensor_->set_global_pause(false);
                if (input_adapter_) input_adapter_->start();
            }
            setSensorCapabilityAvailable(
                sensor_id == kSystemActivitySensorId
                    ? "system.activity_impl" : "interaction.input_impl",
                true, "consent_granted");
        }
        consent_granted_ = consent_store_->all_granted();
    } catch (const std::exception& error) {
        qCritical().noquote()
            << structuredError("desktop_sensor_consent_write_failed", error);
        transitionSession(DesktopSessionPhase::degraded, {},
                          "consent_persistence_failed");
    }
}

void DesktopController::setPaused(bool paused) {
    try {
        {
            std::lock_guard lock(runtime_mutex_);
            paused_ = paused;
            consent_store_->set_paused(paused);
            if (system_sensor_) system_sensor_->set_global_pause(paused);
            if (input_sensor_) input_sensor_->set_global_pause(paused);
            setSensorCapabilityAvailable("system.activity_impl", !paused,
                                         paused ? "global_pause" : "resumed");
            setSensorCapabilityAvailable("interaction.input_impl", !paused,
                                         paused ? "global_pause" : "resumed");
            if (input_adapter_) {
                if (paused) {
                    input_adapter_->stop();
                } else if (sensorAllowed(kInputInteractionSensorId,
                                         kInputInteractionPurpose)) {
                    input_adapter_->start();
                }
            }
        }
        if (paused) {
            tray_adapter_->setPresence(PresenceState::paused, "User paused");
            transitionSession(DesktopSessionPhase::paused);
        } else {
            tray_adapter_->setPresence(model_available_ ? PresenceState::active
                                                        : PresenceState::degraded,
                                       "User resumed");
            transitionSession(model_available_ ? DesktopSessionPhase::running
                                                : DesktopSessionPhase::degraded,
                              {}, model_available_ ? std::nullopt
                                                   : std::optional<std::string>{"model_unavailable"});
        }
    } catch (const std::exception& error) {
        qCritical().noquote()
            << structuredError("desktop_pause_write_failed", error);
        transitionSession(DesktopSessionPhase::degraded, {},
                          "consent_persistence_failed");
    }
}

bool DesktopController::sensorAllowed(const std::string& sensor_id,
                                      const std::string& purpose) const {
    return consent_store_ && consent_store_->capture_allowed(sensor_id, purpose);
}

void DesktopController::setSensorCapabilityAvailable(
    const std::string& implementation_id, bool available,
    const std::string& reason_code) {
    if (!runtime_) return;
    auto& registry = runtime_->capability_registry();
    if (!registry.records().contains(implementation_id)) return;
    const auto current = registry.record(implementation_id).state.state;
    if (available) {
        if (current == CapabilityState::temporarily_unavailable ||
            current == CapabilityState::disabled) {
            registry.transition(implementation_id, CapabilityState::available,
                                reason_code);
        }
        return;
    }
    if (current == CapabilityState::available ||
        current == CapabilityState::degraded) {
        registry.invalidate_for(implementation_id);
        registry.transition(implementation_id,
                            CapabilityState::temporarily_unavailable,
                            reason_code);
    }
}

std::string DesktopController::sessionStateJson() const {
    std::lock_guard lock(session_mutex_);
    return session_state_.to_json();
}

std::uint64_t DesktopController::sensorEventCount() const noexcept {
    return sensor_event_count_.load();
}

bool DesktopController::nativeEventFilter(const QByteArray&, void* message,
                                          qintptr*) {
#ifdef _WIN32
    if (message == nullptr) return false;
    const auto* native_message = static_cast<MSG*>(message);
    if (native_message->message != WM_POWERBROADCAST) return false;
    if (native_message->wParam == PBT_APMSUSPEND) {
        {
            std::lock_guard lock(runtime_mutex_);
            if (system_sensor_) system_sensor_->set_global_pause(true);
            if (input_sensor_) input_sensor_->set_global_pause(true);
            if (input_adapter_) input_adapter_->stop();
        }
        transitionSession(DesktopSessionPhase::degraded, {},
                          "system_suspended");
    } else if (native_message->wParam == PBT_APMRESUMEAUTOMATIC ||
               native_message->wParam == PBT_APMRESUMESUSPEND) {
        {
            std::lock_guard lock(runtime_mutex_);
            if (system_sensor_) system_sensor_->set_global_pause(paused_);
            if (input_sensor_) input_sensor_->set_global_pause(paused_);
            if (input_adapter_ && !paused_ &&
                sensorAllowed(kInputInteractionSensorId,
                              kInputInteractionPurpose)) {
                input_adapter_->start();
            }
        }
        if (paused_) {
            transitionSession(DesktopSessionPhase::paused);
        } else {
            transitionSession(DesktopSessionPhase::degraded, {},
                              model_available_ ? std::nullopt
                                               : std::optional<std::string>{"model_unavailable"});
        }
    }
#else
    (void)message;
#endif
    return false;
}

void DesktopController::transitionSession(
    DesktopSessionPhase next, std::vector<std::string> active_sensor_ids,
    std::optional<std::string> reason_code) {
    bool consent_ready = false;
    {
        std::lock_guard lock(runtime_mutex_);
        consent_ready = consent_store_ && consent_store_->all_granted();
    }
    std::string json;
    {
        std::lock_guard lock(session_mutex_);
        if (!desktop_session_transition_allowed(session_state_.state, next)) {
            qWarning().noquote()
                << QString("{\"event\":\"desktop_transition_rejected\",\"from\":\"%1\",\"to\":\"%2\"}")
                       .arg(QString::fromStdString(
                                desktop_session_phase_name(session_state_.state)),
                            QString::fromStdString(desktop_session_phase_name(next)));
            return;
        }
        session_state_.state = next;
        session_state_.observed_at = desktop_utc_now();
        session_state_.consent_ready = consent_ready;
        session_state_.active_sensor_ids = std::move(active_sensor_ids);
        session_state_.paused = next == DesktopSessionPhase::paused;
        session_state_.previous_shutdown_unclean = previous_shutdown_unclean_;
        session_state_.model_available = model_available_;
        session_state_.reason_code = std::move(reason_code);
        json = session_state_.to_json();
    }
    emit sessionStateUpdated(QString::fromStdString(json));
}

void DesktopController::start() {
    bool expected = false;
    if (!started_.compare_exchange_strong(expected, true)) return;
    tray_adapter_->show();

    QSettings legacy_settings("EU-Digital", "DesktopRuntime");
    legacy_settings.remove("consent_granted");
    legacy_settings.sync();

    try {
        std::lock_guard lock(runtime_mutex_);
        if (!consent_store_->load()) {
            tray_adapter_->setPresence(PresenceState::degraded,
                                       "Ledger de consentimento indisponivel");
            transitionSession(DesktopSessionPhase::degraded, {},
                              consent_store_->error_code());
            health_timer_->start(1000);
            return;
        }
        previous_shutdown_unclean_ = session_marker_->begin(
            session_id_, desktop_utc_now());
        paused_ = consent_store_->paused();
        consent_granted_ = consent_store_->all_granted();
    } catch (const std::exception& error) {
        qCritical().noquote()
            << structuredError("desktop_session_start_failed", error);
        transitionSession(DesktopSessionPhase::degraded, {},
                          "session_storage_unavailable");
        health_timer_->start(1000);
        return;
    }

    if (!consent_store_->any_granted()) {
        auto answer = QMessageBox::No;
        if (config_.show_onboarding) {
            answer = QMessageBox::question(
                nullptr, "Eu Digital - Consentimento de observacao",
                "O Eu Digital pode observar localmente dois fluxos independentes:\n"
                "- atividade do sistema (aplicativos e janela ativa);\n"
                "- interacao de entrada agregada (teclado e ponteiro).\n\n"
                "Nenhum dado e enviado para a nuvem. Autoriza os dois fluxos?",
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        }
        if (answer == QMessageBox::Yes) setConsent(true);
        if (!consent_store_->any_granted()) {
            tray_adapter_->setPresence(PresenceState::paused,
                                       "Aguardando consentimento");
            transitionSession(DesktopSessionPhase::onboarding, {},
                              "consent_required");
            health_timer_->start(1000);
            return;
        }
    }

    if (!std::filesystem::is_regular_file(config_.manifest_path)) {
        tray_adapter_->setPresence(PresenceState::degraded,
                                   "Manifesto do runtime ausente");
        transitionSession(DesktopSessionPhase::degraded, {},
                          "runtime_manifest_unavailable");
        health_timer_->start(1000);
        return;
    }
    transitionSession(DesktopSessionPhase::starting);

    running_ = true;
    cognitive_thread_ = std::jthread([this](std::stop_token stoken) {
        try {
            RuntimeConfig config;
            config.manifest_path = config_.manifest_path.string();
            config.timeline_path =
                (config_.data_directory / "timeline.sqlite").string();
            config.session_id = session_id_;
            config.observed_at = desktop_utc_now();

            std::unique_ptr<RuntimeHost> host =
                std::make_unique<RuntimeHost>(config);
        
        // Initialize Adapters & Sensors
        {
            std::lock_guard lock(runtime_mutex_);
            if (config_.enable_real_sensors &&
                sensorAllowed(kSystemActivitySensorId, kSystemActivityPurpose)) {
                activity_adapter_ =
                    std::make_unique<WindowsSystemActivityAdapter>(false);
                system_sensor_ = std::make_shared<SystemActivitySensor>(
                    *activity_adapter_, [this](const CanonicalEvent& event) {
                        std::lock_guard lock(runtime_mutex_);
                        if (!sensorAllowed(kSystemActivitySensorId,
                                           kSystemActivityPurpose) || !runtime_) {
                            return;
                        }
                        try {
                            runtime_->event_bus().publish(event);
                            ++sensor_event_count_;
                        } catch (const std::exception& error) {
                            qWarning().noquote()
                                << structuredError("desktop_sensor_publish_failed",
                                                   error, "system_activity");
                        }
                    });
                system_sensor_->set_global_pause(paused_);
                host->capability_registry().register_instance(
                    "system.activity", system_sensor_);
            }

            if (config_.enable_real_sensors &&
                sensorAllowed(kInputInteractionSensorId,
                              kInputInteractionPurpose)) {
                input_adapter_ = std::make_unique<WindowsInputCaptureAdapter>(
                    [this](const RawInputEvent& input,
                           const WindowContext& context) {
                        std::lock_guard lock(runtime_mutex_);
                        if (sensorAllowed(kInputInteractionSensorId,
                                          kInputInteractionPurpose) && input_sensor_) {
                            input_sensor_->ingest(input, context);
                        }
                    },
                    false);
                input_sensor_ = std::make_shared<InputInteractionSensor>(
                    [this](const CanonicalEvent& event) {
                        std::lock_guard lock(runtime_mutex_);
                        if (!sensorAllowed(kInputInteractionSensorId,
                                           kInputInteractionPurpose) || !runtime_) {
                            return;
                        }
                        try {
                            runtime_->event_bus().publish(event);
                            ++sensor_event_count_;
                        } catch (const std::exception& error) {
                            qWarning().noquote()
                                << structuredError("desktop_sensor_publish_failed",
                                                   error, "input_interaction");
                        }
                    });
                input_sensor_->set_global_pause(paused_);
                host->capability_registry().register_instance(
                    "interaction.input", input_sensor_);
            }
            
            // Instantiate the SPEC-045 cognitive cycle and SPEC-048 output ports.
            episodic_memory_ = std::make_shared<EpisodicMemoryStore>();
            pattern_learner_ = std::make_shared<PatternLearner>(
                PatternConfig{}, "desktop-patterns");
            
            WorldModelConfig wm_config;
            world_model_ = std::make_shared<WorldModel>(wm_config, "desktop");
            
            global_workspace_ = std::make_shared<GlobalWorkspace>(
                "gw-desktop", config.session_id, WorkspaceConfig{});
            
            self_model_ = std::make_shared<VersionedFunctionalSelfModel>(
                "eu-digital-desktop", config.observed_at);
            
            metacognition_engine_ = std::make_shared<MetacognitionCuriosityEngine>();
            suggestion_orchestrator_ = std::make_shared<SuggestionOrchestrator>();
            
            auto episode_adapter = std::make_shared<EpisodeSegmenterAdapter>(
                "episode_boundary_impl");
            CapabilityDescriptor episode_descriptor;
            episode_descriptor.capability_id = "cognition.episode_boundary";
            episode_descriptor.implementation_id = "episode_boundary_impl";
            episode_descriptor.implementation_version = "1.0.0";
            episode_descriptor.kind = "cognitive_service";
            episode_descriptor.provides.push_back(
                {"episode_boundary", "urn:eu-digital:episode-boundary:1"});
            episode_descriptor.supports_hot_plug = true;
            episode_descriptor.supports_checkpoint = true;
            host->capability_registry().register_instance<IEpisodeBoundaryPort>(
                std::move(episode_descriptor), episode_adapter, 10);

            CapabilityDescriptor episode_state_descriptor;
            episode_state_descriptor.capability_id =
                "cognition.episode_boundary.state";
            episode_state_descriptor.implementation_id =
                "episode_boundary_state_port";
            episode_state_descriptor.implementation_version = "1.0.0";
            episode_state_descriptor.kind = "cognitive_state_port";
            episode_state_descriptor.provides.push_back(
                {"cognitive_state", "urn:eu-digital:cognitive-state:1"});
            episode_state_descriptor.supports_hot_plug = true;
            episode_state_descriptor.supports_checkpoint = true;
            host->capability_registry().register_instance<ICognitiveStatePort>(
                std::move(episode_state_descriptor), episode_adapter, 10);
            host->capability_registry().register_instance("memory_write",
                CognitivePortFactory::create_memory_write_port(episodic_memory_));

            CapabilityDescriptor pattern_descriptor;
            pattern_descriptor.capability_id = "cognition.pattern_learning";
            pattern_descriptor.implementation_id = "native.pattern_learning.desktop";
            pattern_descriptor.implementation_version = "1.0.0";
            pattern_descriptor.kind = "cognitive_service";
            pattern_descriptor.provides.push_back(
                {"learn.patterns", "urn:eu-digital:pattern:1"});
            pattern_descriptor.supports_hot_plug = true;
            host->capability_registry().register_instance(
                std::move(pattern_descriptor),
                CognitivePortFactory::create_pattern_learning_port(pattern_learner_), 10);

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

            CapabilityDescriptor renderer_descriptor;
            renderer_descriptor.capability_id = "dialogue.local_renderer";
            renderer_descriptor.implementation_id =
                "local_language_renderer.safe_fallback";
            renderer_descriptor.implementation_version = "1.0.0";
            renderer_descriptor.kind = "language_renderer";
            renderer_descriptor.supports_hot_plug = true;
            renderer_descriptor.provides.push_back(
                {kLanguageRenderOperation,
                 "urn:eu-digital:contracts:cognitive-output:1.0"});
            auto safe_renderer = std::make_shared<LocalLanguageRenderer>(
                LocalLanguageRenderer::GenerationFunction{});
            host->capability_registry().register_instance<ILanguageRenderer>(
                std::move(renderer_descriptor), safe_renderer, 1);

            CapabilityDescriptor presentation_descriptor;
            presentation_descriptor.capability_id = "presentation.qt_dialogue";
            presentation_descriptor.implementation_id =
                "qt_dialogue_presentation";
            presentation_descriptor.implementation_version = "1.0.0";
            presentation_descriptor.kind = "presentation";
            presentation_descriptor.supports_hot_plug = true;
            presentation_descriptor.provides.push_back(
                {kPresentationOperation,
                 "urn:eu-digital:contracts:cognitive-output:1.0"});
            auto qt_presentation =
                std::make_shared<QtDialoguePresentationAdapter>(
                    this, [this](const QString& text) {
                        QMetaObject::invokeMethod(
                            this,
                            [this, text]() {
                                appendMessageToTray("agent", text);
                                if (tray_adapter_) {
                                    tray_adapter_->setPresence(
                                        PresenceState::active);
                                }
                            },
                            Qt::QueuedConnection);
                    });
            host->capability_registry().register_instance<IPresentationPort>(
                std::move(presentation_descriptor), qt_presentation, 10);
            
            runtime_ = std::move(host);
        }

        if (!runtime_->start()) {
            throw RuntimeHostError("desktop runtime failed to start");
        }

        if (!running_ || stoken.stop_requested()) {
            cleanupRuntimeComponents();
            return;
        }

        std::vector<std::string> active_sensor_ids;
        {
            std::lock_guard lock(runtime_mutex_);
            if (system_sensor_) active_sensor_ids.push_back(kSystemActivitySensorId);
            if (input_sensor_) active_sensor_ids.push_back(kInputInteractionSensorId);
            if (input_adapter_ && !paused_) input_adapter_->start();
        }

        model_available_ = false;
        if (paused_) {
            transitionSession(DesktopSessionPhase::paused);
        } else {
            const auto reason = previous_shutdown_unclean_
                ? "previous_shutdown_unclean"
                : "model_unavailable";
            transitionSession(DesktopSessionPhase::degraded,
                              std::move(active_sensor_ids), reason);
            QMetaObject::invokeMethod(
                this,
                [this, reason]() {
                    tray_adapter_->setPresence(PresenceState::degraded, reason);
                    if (previous_shutdown_unclean_) {
                        tray_adapter_->showNotification(
                            "Recuperacao local concluida",
                            "Uma sessao interrompida foi detectada; a timeline local foi verificada.",
                            QSystemTrayIcon::Warning);
                    }
                },
                Qt::QueuedConnection);
        }

        auto next_system_poll = std::chrono::steady_clock::now();
        while (running_ && !stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            
            // Tick Sensors
            {
                std::lock_guard lock(runtime_mutex_);
                if (input_adapter_) input_adapter_->pump_once();
                if (system_sensor_ &&
                    std::chrono::steady_clock::now() >= next_system_poll) {
                    system_sensor_->poll();
                    next_system_poll = std::chrono::steady_clock::now() +
                                       std::chrono::seconds(1);
                }
            }
        }

        cleanupRuntimeComponents();
        } catch (const std::exception& e) {
            cleanupRuntimeComponents();
            qCritical().noquote()
                << structuredError("desktop_cognitive_thread_failed", e);
            if (running_) {
                running_ = false;
                transitionSession(DesktopSessionPhase::degraded, {},
                                  "runtime_start_failed");
            }
        }
    });

    health_timer_->start(1000);
}

void DesktopController::cleanupRuntimeComponents() {
    std::lock_guard lock(runtime_mutex_);
    if (input_adapter_) input_adapter_->stop();
    if (system_sensor_) system_sensor_->set_global_pause(true);
    if (input_sensor_) input_sensor_->set_global_pause(true);
    if (runtime_) {
        runtime_->stop();
        runtime_.reset();
    }
    system_sensor_.reset();
    activity_adapter_.reset();
    input_sensor_.reset();
    input_adapter_.reset();
}

void DesktopController::stop() {
    if (!started_) return;
    transitionSession(DesktopSessionPhase::stopping);
    {
        std::lock_guard lock(runtime_mutex_);
        if (system_sensor_) system_sensor_->set_global_pause(true);
        if (input_sensor_) input_sensor_->set_global_pause(true);
        if (input_adapter_) input_adapter_->stop();
    }
    running_ = false;
    if (cognitive_thread_.joinable()) {
        cognitive_thread_.request_stop();
        cognitive_thread_.join();
    }
    cleanupRuntimeComponents();
    health_timer_->stop();
    try {
        session_marker_->finish_clean();
    } catch (const std::exception& error) {
        qCritical().noquote()
            << structuredError("desktop_session_marker_remove_failed", error);
        transitionSession(DesktopSessionPhase::degraded, {},
                          "session_marker_remove_failed");
        return;
    }
    transitionSession(DesktopSessionPhase::stopped);
    started_ = false;
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
            event.event_type    = "user_explicit_question";
            event.schema_version = "1.0";
            QJsonObject payload_obj;
            payload_obj["text"] = text;
            QJsonDocument payload_doc(payload_obj);
            event.payload = payload_doc.toJson(QJsonDocument::Compact).toStdString();
            try {
                runtime_->event_bus().publish(event);
            } catch (const std::exception& error) {
                qWarning().noquote()
                    << structuredError("desktop_user_input_publish_failed", error);
            }
        }
    }

    // Show a "thinking" indicator while the cognitive cycle processes
    appendMessageToTray("agent", "\xF0\x9F\xA4\x94 Processando...");
    tray_adapter_->setPresence(PresenceState::processing, "Processando ciclo cognitivo");
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
