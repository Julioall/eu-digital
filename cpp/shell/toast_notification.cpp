#include "toast_notification.hpp"
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QDateTime>
#include <QPropertyAnimation>

namespace eu_digital {

ToastNotification::ToastNotification(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon type, QWidget* parent) 
    : QWidget(parent), title_(title), body_(body), type_(type) {
    
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setFixedSize(360, 90);
    
    setupUi();
    
    close_timer_ = new QTimer(this);
    close_timer_->setSingleShot(true);
    close_timer_->setInterval(5000); // 5 seconds
    connect(close_timer_, &QTimer::timeout, this, &ToastNotification::close);
}

void ToastNotification::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#1A1A1E"));
    p.setPen(QPen(QColor("#2A2A2E"), 1));
    p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 12, 12);
}

void ToastNotification::setupUi() {
    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(16, 12, 16, 12);
    main_layout->setSpacing(12);

    // Ícone da esquerda
    QLabel* icon_lbl = new QLabel(this);
    icon_lbl->setFixedSize(32, 32);
    QString glyph;
    QString color;
    
    switch (type_) {
        case QSystemTrayIcon::Information: glyph = QString::fromUtf8("\xE2\x84\xB9"); color = "#3B82F6"; break; // Info
        case QSystemTrayIcon::Warning: glyph = QString::fromUtf8("\xE2\x9A\xA0"); color = "#F59E0B"; break; // Warning
        case QSystemTrayIcon::Critical: glyph = QString::fromUtf8("\xE2\x9B\x94"); color = "#EF4444"; break; // Critical
        default: glyph = QString::fromUtf8("\xF0\x9F\x94\x94"); color = "#3B82F6"; break; // Bell
    }
    
    icon_lbl->setStyleSheet(QString("background-color: %1; color: white; border-radius: 16px; font-size: 16px;").arg(QColor(color).darker(150).name()));
    icon_lbl->setAlignment(Qt::AlignCenter);
    icon_lbl->setText(glyph);

    // Textos (Título e Corpo)
    QVBoxLayout* text_layout = new QVBoxLayout();
    text_layout->setSpacing(2);
    
    QLabel* title_lbl = new QLabel(title_, this);
    title_lbl->setStyleSheet("color: white; font-weight: bold; font-size: 13px; font-family: 'Segoe UI', Arial;");
    
    QLabel* body_lbl = new QLabel(body_, this);
    body_lbl->setStyleSheet("color: #A0A0A0; font-size: 12px; font-family: 'Segoe UI', Arial;");
    body_lbl->setWordWrap(true);
    
    text_layout->addWidget(title_lbl);
    text_layout->addWidget(body_lbl);
    text_layout->addStretch();
    
    // Top Right (Time e Close)
    QVBoxLayout* right_layout = new QVBoxLayout();
    right_layout->setSpacing(2);
    
    QHBoxLayout* top_right = new QHBoxLayout();
    QLabel* time_lbl = new QLabel(QDateTime::currentDateTime().toString("hh:mm"), this);
    time_lbl->setStyleSheet("color: #6B7280; font-size: 11px;");
    
    QPushButton* close_btn = new QPushButton(QString::fromUtf8("\xE2\x95\xB3"), this);
    close_btn->setFixedSize(16, 16);
    close_btn->setStyleSheet("QPushButton { background: transparent; color: #A0A0A0; border: none; font-size: 10px; } QPushButton:hover { color: white; }");
    connect(close_btn, &QPushButton::clicked, this, &ToastNotification::close);
    
    top_right->addWidget(time_lbl);
    top_right->addWidget(close_btn);
    
    right_layout->addLayout(top_right);
    right_layout->addStretch();
    
    main_layout->addWidget(icon_lbl, 0, Qt::AlignTop);
    main_layout->addLayout(text_layout);
    main_layout->addLayout(right_layout);
    
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);
}

void ToastNotification::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    close_timer_->start();
}

void ToastNotification::positionAndShow() {
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    
    // Animando da direita para a esquerda embaixo
    int startX = screenGeometry.right();
    int endX = screenGeometry.right() - width() - 20;
    int y = screenGeometry.bottom() - height() - 20;
    
    move(startX, y);
    show();
    
    QPropertyAnimation* anim = new QPropertyAnimation(this, "pos");
    anim->setDuration(300);
    anim->setStartValue(QPoint(startX, y));
    anim->setEndValue(QPoint(endX, y));
    anim->setEasingCurve(QEasingCurve::OutBack);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastNotification::showToast(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon type) {
    ToastNotification* toast = new ToastNotification(title, body, type);
    toast->positionAndShow();
}

} // namespace eu_digital
