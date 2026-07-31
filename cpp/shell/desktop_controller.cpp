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
    
    connect(tray_adapter_.get(), &QtTrayAdapter::pauseRequested, this, &DesktopController::onTrayPauseRequested);
    connect(tray_adapter_.get(), &QtTrayAdapter::consentRevoked, this, &DesktopController::onTrayConsentRevoked);

    health_timer_ = new QTimer(this);
    connect(health_timer_, &QTimer::timeout, this, &DesktopController::checkHealth);
}

DesktopController::~DesktopController() {
    stop();
}

bool DesktopController::checkConsent() {
    QSettings settings("EU-Digital", "DesktopRuntime");
    if (!settings.contains("consent_granted")) {
        // Show Onboarding Modal
        QMessageBox msgBox;
        msgBox.setWindowTitle("EU-Digital: Onboarding");
        msgBox.setText("Bem-vindo ao EU-Digital.\n\nPara prosseguir, precisamos do seu consentimento para observação e sensoriamento das suas interações, garantindo total privacidade e armazenamento local.");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        msgBox.setButtonText(QMessageBox::Yes, "Conceder Consentimento");
        msgBox.setButtonText(QMessageBox::No, "Recusar e Fechar");

        int ret = msgBox.exec();
        if (ret == QMessageBox::Yes) {
            settings.setValue("consent_granted", true);
        } else {
            return false; // Blocks the app
        }
    }
    
    consent_granted_ = settings.value("consent_granted").toBool();
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
    // We would propagate this to sensors/privacy policy if we managed them directly here.
    // For now, it simply updates the state.
}

void DesktopController::start() {
    if (!checkConsent()) {
        QCoreApplication::quit();
        return;
    }

    tray_adapter_->show();

    running_ = true;
    cognitive_thread_ = std::jthread([this](std::stop_token stoken) {
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
        host->start();

        {
            std::lock_guard lock(runtime_mutex_);
            runtime_ = std::move(host);
        }

        while (running_ && !stoken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // In a real implementation we would tick sensors here.
            // if (!paused_) { sensor->poll(); }
        }

        {
            std::lock_guard lock(runtime_mutex_);
            if (runtime_) {
                runtime_->stop();
                runtime_.reset();
            }
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

void DesktopController::onTrayConsentRevoked(bool revoked) {
    setConsent(!revoked);
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
