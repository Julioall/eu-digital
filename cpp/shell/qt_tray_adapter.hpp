#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>

namespace eu_digital {

/// Qt System Tray Adapter (SPEC-042).
/// Provides menu with pause/resume, consent toggle, and health status.
class QtTrayAdapter : public QObject {
    Q_OBJECT
public:
    explicit QtTrayAdapter(QObject* parent = nullptr) : QObject(parent) {
        tray_icon_ = new QSystemTrayIcon(this);
        
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::darkBlue);
        tray_icon_->setIcon(QIcon(pixmap));
        tray_menu_ = new QMenu();

        action_pause_ = new QAction("Pause Avatar", this);
        action_consent_ = new QAction("Revoke Consent", this);
        action_health_ = new QAction("Health: OK", this);
        action_health_->setEnabled(false); // Status only

        tray_menu_->addAction(action_health_);
        tray_menu_->addSeparator();
        tray_menu_->addAction(action_pause_);
        tray_menu_->addAction(action_consent_);

        tray_icon_->setContextMenu(tray_menu_);
        
        connect(action_pause_, &QAction::triggered, this, &QtTrayAdapter::onPauseToggled);
        connect(action_consent_, &QAction::triggered, this, &QtTrayAdapter::onConsentToggled);
    }
    
    void show() { tray_icon_->show(); }

signals:
    void pauseRequested(bool paused);
    void consentRevoked(bool revoked);

private slots:
    void onPauseToggled() {
        bool currently_paused = action_pause_->text() == "Resume Avatar";
        action_pause_->setText(currently_paused ? "Pause Avatar" : "Resume Avatar");
        emit pauseRequested(!currently_paused);
    }
    
    void onConsentToggled() {
        bool currently_revoked = action_consent_->text() == "Grant Consent";
        action_consent_->setText(currently_revoked ? "Revoke Consent" : "Grant Consent");
        emit consentRevoked(!currently_revoked);
    }

private:
    QSystemTrayIcon* tray_icon_;
    QMenu* tray_menu_;
    QAction* action_pause_;
    QAction* action_consent_;
    QAction* action_health_;
};

}  // namespace eu_digital
