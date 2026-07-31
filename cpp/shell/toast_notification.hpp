#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QSystemTrayIcon>

namespace eu_digital {

class ToastNotification : public QWidget {
    Q_OBJECT
public:
    explicit ToastNotification(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon type, QWidget* parent = nullptr);
    static void showToast(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon type = QSystemTrayIcon::Information);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    void positionAndShow();

    QString title_;
    QString body_;
    QSystemTrayIcon::MessageIcon type_;
    QTimer* close_timer_;
};

} // namespace eu_digital
