#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include "tray_widget.hpp"
#include "tray_state_machine.hpp"
#include "toast_notification.hpp"

namespace eu_digital {

/// Qt System Tray Adapter (SPEC-052).
/// Manages icon with 6 presence states, RF-02 context menu, and TrayWidget popup.
class QtTrayAdapter : public QObject {
    Q_OBJECT
public:
    explicit QtTrayAdapter(QObject* parent = nullptr) : QObject(parent) {
        tray_icon_     = new QSystemTrayIcon(this);
        tray_widget_   = new TrayWidget();
        state_machine_ = new TrayStateMachine(this);

        buildContextMenu();
        setPresenceIcon(PresenceState::offline);
        tray_icon_->setToolTip("Eu Digital — offline");

        connect(tray_icon_, &QSystemTrayIcon::activated,
                this, &QtTrayAdapter::onTrayIconActivated);
        connect(tray_widget_, &TrayWidget::userInputReceived,
                this, &QtTrayAdapter::userInputReceived);
        connect(state_machine_, &TrayStateMachine::presenceChanged,
                this, &QtTrayAdapter::onPresenceChanged);
                
        connect(tray_widget_, &TrayWidget::settingsRequested,
                this, &QtTrayAdapter::onSettings);
        connect(tray_widget_, &TrayWidget::expandRequested,
                this, [this]() { state_machine_->requestSurface(SurfaceMode::expanded); });
        connect(tray_widget_, &TrayWidget::cancelRequested,
                this, [this]() { /* handle cancel internally or emit */ });
    }

    ~QtTrayAdapter() { delete tray_widget_; }

    void show() { tray_icon_->show(); }
    void activateAt(const QPoint& position) {
        state_machine_->requestSurface(SurfaceMode::compact);
        tray_widget_->toggleVisibility(position);
    }
    TrayWidget*       getTrayWidget()   const { return tray_widget_; }
    TrayStateMachine* getStateMachine() const { return state_machine_; }

    void setPresence(PresenceState state, const QString& reason = {}) {
        state_machine_->requestPresence(state, reason);
    }

    void showNotification(const QString& title, const QString& body,
                          QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information) {
        ToastNotification::showToast(title, body, icon);
    }

signals:
    void pauseRequested(bool paused);
    void muteRequested(bool muted);
    void consentRevoked(bool revoked);
    void userInputReceived(const QString& text);
    void openSettingsRequested();
    void openQuickPanelRequested();
    void shutdownRequested();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            activateAt(QCursor::pos());
        }
    }

    void onPresenceChanged(PresenceState state) {
        setPresenceIcon(state);
        tray_icon_->setToolTip(QString("Eu Digital — %1").arg(presenceStateName(state)));
        if (tray_widget_->isVisible()) {
            tray_widget_->setPresenceState(state);
        }
        action_pause_->setText(state == PresenceState::paused
            ? "Retomar sensores" : "Pausar sensores");
    }

    void onPauseAction()  { emit pauseRequested(state_machine_->presence() != PresenceState::paused); }
    void onMuteAction()   {
        muted_ = !muted_;
        emit muteRequested(muted_);
        action_mute_->setText(muted_ ? "Reativar perguntas" : "Silenciar perguntas");
    }
    void onOpenWidget()   {
        state_machine_->requestSurface(SurfaceMode::compact);
        tray_widget_->toggleVisibility(QCursor::pos());
    }
    void onQuickPanel()   { emit openQuickPanelRequested(); }
    void onSettings()     { emit openSettingsRequested(); }
    void onShutdown()     { emit shutdownRequested(); }

private:
    // ── Icon generation (SPEC-052 RF-01) ──────────────────────────────────
    // Each state uses a DISTINCT SHAPE so it is never identified by color alone.
    void setPresenceIcon(PresenceState state) {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);

        // Desenhar a base da Nuvem (Cloud outline)
        QPainterPath cloud;
        cloud.addEllipse(4, 14, 10, 10);
        cloud.addEllipse(10, 8, 14, 14);
        cloud.addEllipse(20, 12, 10, 10);
        
        QPainterPath bottomRect;
        bottomRect.addRect(9, 14, 16, 10);
        cloud = cloud.united(bottomRect);

        p.setPen(QPen(QColor("#A0A0A0"), 2));
        p.setBrush(Qt::NoBrush);
        
        if (state == PresenceState::offline || state == PresenceState::degraded) {
            p.setPen(QPen(QColor("#6B7280"), 2, Qt::DashLine));
        }
        
        p.drawPath(cloud);

        // Desenhar os badges de estado na parte inferior direita (x:18, y:18)
        p.setPen(Qt::NoPen);
        switch (state) {
            case PresenceState::active: {
                p.setBrush(QColor("#3B82F6"));
                p.drawEllipse(20, 20, 8, 8);
                break;
            }
            case PresenceState::processing: {
                p.setBrush(QColor("#3B82F6"));
                p.drawEllipse(16, 22, 4, 4);
                p.drawEllipse(21, 22, 4, 4);
                p.drawEllipse(26, 22, 4, 4);
                break;
            }
            case PresenceState::asking: {
                p.setBrush(QColor("#3B82F6"));
                p.drawEllipse(18, 18, 12, 12);
                p.setPen(QPen(Qt::white, 2));
                p.setFont(QFont("Arial", 8, QFont::Bold));
                p.drawText(QRect(18,18,12,12), Qt::AlignCenter, "?");
                break;
            }
            case PresenceState::paused: {
                p.setBrush(QColor("#3B82F6"));
                p.drawEllipse(18, 18, 12, 12);
                p.setBrush(Qt::white); p.setPen(Qt::NoPen);
                p.drawRect(21, 21, 2, 6);
                p.drawRect(25, 21, 2, 6);
                break;
            }
            case PresenceState::offline:
            case PresenceState::degraded: {
                // Nuvem tracejada apenas, sem badge azul
                break;
            }
        }
        p.end();
        tray_icon_->setIcon(QIcon(pix));
    }

    QIcon createMenuIcon(const QString& glyph, QColor color = QColor("#A0A0A0")) {
        QPixmap pix(24, 24);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(color);
        p.setFont(QFont("Segoe UI Symbol", 14));
        p.drawText(pix.rect(), Qt::AlignCenter, glyph);
        return QIcon(pix);
    }

    // ── Context menu (SPEC-052 RF-02) ─────────────────────────────────────
    void buildContextMenu() {
        auto* menu = new QMenu();
        // High fidelity styling for menu
        menu->setStyleSheet(
            "QMenu { background-color: #121215; color: white; border: 1px solid #232328; "
            "        border-radius: 8px; padding: 6px; font-size: 13px; font-family: 'Segoe UI', Arial, sans-serif; }"
            "QMenu::item { padding: 8px 32px 8px 12px; border-radius: 6px; }"
            "QMenu::item:selected { background-color: #1E1E22; }"
            "QMenu::item:disabled { color: #555555; }"
            "QMenu::separator { height: 1px; background-color: #232328; margin: 6px 0; }"
        );

        auto* open_act = menu->addAction(createMenuIcon(QString::fromUtf8("\xE2\x8C\x82")), "Abrir Eu Digital");
        connect(open_act, &QAction::triggered, this, &QtTrayAdapter::onOpenWidget);



        menu->addSeparator();

        action_pause_ = menu->addAction(createMenuIcon(QString::fromUtf8("\xE2\x8F\xB8")), "Pausar Sensores");
        connect(action_pause_, &QAction::triggered, this, &QtTrayAdapter::onPauseAction);

        action_mute_ = menu->addAction(createMenuIcon(QString::fromUtf8("\xF0\x9F\x94\x95")), "Silenciar Perguntas");
        connect(action_mute_, &QAction::triggered, this, &QtTrayAdapter::onMuteAction);

        menu->addSeparator();

        auto* settings_act = menu->addAction(createMenuIcon(QString::fromUtf8("\xE2\x9A\x99")), "Configurações");
        connect(settings_act, &QAction::triggered, this, &QtTrayAdapter::onSettings);

        auto* mem_act = menu->addAction(createMenuIcon(QString::fromUtf8("\xE2\x96\xA4")), "Ver Memórias");
        mem_act->setEnabled(false); // mock

        menu->addSeparator();

        auto* exit_act = menu->addAction(createMenuIcon(QString::fromUtf8("\xE2\x8F\xBB"), QColor("#EF4444")), "Encerrar");
        connect(exit_act, &QAction::triggered, this, [this]() { emit shutdownRequested(); });

        tray_icon_->setContextMenu(menu);
    }

    QSystemTrayIcon*  tray_icon_;
    TrayWidget*       tray_widget_;
    TrayStateMachine* state_machine_;
    QAction*          action_pause_{nullptr};
    QAction*          action_mute_{nullptr};

    bool              muted_{false};
};

}  // namespace eu_digital
