#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QString>
#include <QEvent>
#include "tray_state_machine.hpp"

namespace eu_digital {

class TrayWidget : public QWidget {
    Q_OBJECT
public:
    explicit TrayWidget(QWidget* parent = nullptr);

    void setStatusText(const QString& text);
    void appendMessage(const QString& role, const QString& text);
    void toggleVisibility(const QPoint& trayIconPos);
    void setPresenceState(PresenceState state);

signals:
    void userInputReceived(const QString& text);
    void expandRequested();
    void cancelRequested();
    void settingsRequested();

protected:
    bool event(QEvent* e) override;

private slots:
    void onInputReturnPressed();
    void onExpandClicked();
    void onCancelClicked();
    void onSettingsClicked();

private:
    void setupUi();
    void expandWidget();
    void shrinkWidget();

    bool is_expanded_{false};

    QLabel* avatar_label_;
    QLabel* status_label_;
    QTextBrowser* chat_history_;
    QLineEdit* input_field_;
    QPushButton* expand_btn_;
    QPushButton* cancel_btn_;
    QPushButton* settings_btn_;
    QVBoxLayout* main_layout_;

};

} // namespace eu_digital
