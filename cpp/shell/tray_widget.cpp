#include "tray_widget.hpp"
#include <QScreen>
#include <QGuiApplication>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>

namespace eu_digital {

// Helper to draw the cloud icon for the header
class CloudIconWidget : public QWidget {
public:
    CloudIconWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(36, 36);
    }
    void setDotColor(const QColor& c) { dot_color = c; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        QPainterPath cloud;
        cloud.addEllipse(4, 16, 12, 12);
        cloud.addEllipse(12, 10, 14, 14);
        cloud.addEllipse(22, 14, 10, 10);
        QPainterPath bottomRect;
        bottomRect.addRect(10, 16, 17, 12);
        cloud = cloud.united(bottomRect);

        p.setPen(QPen(QColor("#A0A0A0"), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(cloud);
        
        // Active dot
        if (dot_color.isValid()) {
            p.setPen(Qt::NoPen);
            p.setBrush(dot_color);
            p.drawEllipse(22, 22, 8, 8);
        }
    }
private:
    QColor dot_color{"#3B82F6"};
};

TrayWidget::TrayWidget(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    setupUi();
    shrinkWidget();
}

void TrayWidget::setupUi() {
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setContentsMargins(16, 16, 16, 16);
    main_layout_->setSpacing(0);

    // Background Container
    QWidget* container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet(
        "#container { "
        "   background-color: #121215; " // Very dark background like the mockup
        "   border-radius: 16px; "
        "   border: 1px solid #232328; "
        "}"
    );

    QVBoxLayout* container_layout = new QVBoxLayout(container);
    container_layout->setContentsMargins(16, 16, 16, 16);
    container_layout->setSpacing(16);

    // Header
    QHBoxLayout* header_layout = new QHBoxLayout();
    
    auto* cloud_icon = new CloudIconWidget(container);
    
    QVBoxLayout* titles_layout = new QVBoxLayout();
    titles_layout->setSpacing(0);
    QLabel* title_label = new QLabel("Eu Digital", container);
    title_label->setStyleSheet("color: white; font-weight: bold; font-size: 14px; font-family: 'Segoe UI', Arial, sans-serif;");
    
    status_label_ = new QLabel("Ativo e monitorando", container);
    status_label_->setStyleSheet("color: #A0A0A0; font-size: 12px; font-family: 'Segoe UI', Arial, sans-serif;");

    titles_layout->addWidget(title_label);
    titles_layout->addWidget(status_label_);
    titles_layout->addStretch();
    
    expand_btn_ = new QPushButton("-", container);
    expand_btn_->setFixedSize(28, 28);
    expand_btn_->setStyleSheet("QPushButton { background: #1E1E22; color: #A0A0A0; border-radius: 14px; font-weight: bold; font-size: 14px; }"
                               "QPushButton:hover { background: #2A2A2E; color: white; }");
    connect(expand_btn_, &QPushButton::clicked, this, &TrayWidget::onExpandClicked);

    settings_btn_ = new QPushButton("⚙", container);
    settings_btn_->setFixedSize(28, 28);
    settings_btn_->setStyleSheet("QPushButton { background: transparent; color: #A0A0A0; border: none; font-size: 16px; }"
                                 "QPushButton:hover { color: white; }");
    connect(settings_btn_, &QPushButton::clicked, this, &TrayWidget::onSettingsClicked);
    settings_btn_->hide(); // Hidden in mockup layout, accessible via context menu, but keeping it optional

    avatar_label_ = new QLabel("ED", container);
    avatar_label_->setFixedSize(36, 36);
    avatar_label_->setAlignment(Qt::AlignCenter);
    avatar_label_->setStyleSheet(
        "QLabel { "
        "   background-color: #1E3A8A; " // Dark blue background for ED
        "   color: #60A5FA; "
        "   border-radius: 18px; "
        "   font-weight: bold; "
        "   font-size: 14px; "
        "}"
    );

    header_layout->addWidget(cloud_icon);
    header_layout->addSpacing(8);
    header_layout->addLayout(titles_layout);
    header_layout->addStretch();
    header_layout->addWidget(expand_btn_);
    header_layout->addSpacing(8);
    header_layout->addWidget(avatar_label_);

    // Chat History
    chat_history_ = new QTextBrowser(container);
    chat_history_->setStyleSheet(
        "QTextBrowser { "
        "   background-color: transparent; "
        "   color: white; "
        "   border: none; "
        "   font-size: 13px; "
        "   font-family: 'Segoe UI', Arial, sans-serif; "
        "}"
    );
    chat_history_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chat_history_->hide();

    // Input Field + Send Button
    QHBoxLayout* input_layout = new QHBoxLayout();
    input_layout->setSpacing(8);
    
    input_field_ = new QLineEdit(container);
    input_field_->setPlaceholderText("Pergunte algo rápido...");
    input_field_->setStyleSheet(
        "QLineEdit { "
        "   background-color: #1E1E22; "
        "   color: white; "
        "   border-radius: 8px; "
        "   border: 1px solid #2A2A2E; "
        "   padding: 10px 14px; "
        "   font-size: 13px; "
        "   font-family: 'Segoe UI', Arial, sans-serif; "
        "}"
        "QLineEdit:focus { border: 1px solid #3B82F6; }"
    );
    connect(input_field_, &QLineEdit::returnPressed, this, &TrayWidget::onInputReturnPressed);

    cancel_btn_ = new QPushButton(QString::fromUtf8("\xE2\x9E\xA4"), container); // Send arrow
    cancel_btn_->setFixedSize(36, 36);
    cancel_btn_->setStyleSheet("QPushButton { background: #3B82F6; color: white; border: none; border-radius: 8px; font-size: 18px; }"
                               "QPushButton:hover { background: #2563EB; }");
    connect(cancel_btn_, &QPushButton::clicked, this, &TrayWidget::onInputReturnPressed);

    input_layout->addWidget(input_field_);
    input_layout->addWidget(cancel_btn_);

    container_layout->addLayout(header_layout);

    // SPEC-053: Current activity area (shown in compact mode)
    QWidget* activity_area = new QWidget(container);
    activity_area->setStyleSheet("QWidget { background-color: #1A1A2E; border-radius: 8px; }");
    QHBoxLayout* activity_layout = new QHBoxLayout(activity_area);
    activity_layout->setContentsMargins(12, 8, 12, 8);

    QLabel* activity_icon = new QLabel(QString::fromUtf8("\xF0\x9F\x92\xBB"), activity_area);
    activity_icon->setStyleSheet("font-size: 16px;");
    activity_icon->setFixedWidth(24);

    QVBoxLayout* activity_text_layout = new QVBoxLayout();
    activity_text_layout->setSpacing(0);
    activity_label_ = new QLabel("Nenhuma atividade detectada", activity_area);
    activity_label_->setStyleSheet("color: #E2E8F0; font-size: 12px; font-family: 'Segoe UI', Arial;");
    activity_duration_label_ = new QLabel("", activity_area);
    activity_duration_label_->setStyleSheet("color: #64748B; font-size: 11px; font-family: 'Segoe UI', Arial;");
    activity_text_layout->addWidget(activity_label_);
    activity_text_layout->addWidget(activity_duration_label_);

    activity_layout->addWidget(activity_icon);
    activity_layout->addLayout(activity_text_layout);
    activity_layout->addStretch();

    container_layout->addWidget(activity_area);

    // SPEC-053: Contextual assistance card (hidden by default)
    assistance_card_widget_ = new QWidget(container);
    assistance_card_widget_->setStyleSheet(
        "QWidget { background-color: #1E293B; border-radius: 8px; border: 1px solid #334155; }");
    QVBoxLayout* card_layout = new QVBoxLayout(assistance_card_widget_);
    card_layout->setContentsMargins(12, 10, 12, 10);
    card_layout->setSpacing(4);

    card_title_label_ = new QLabel("", assistance_card_widget_);
    card_title_label_->setStyleSheet("color: #93C5FD; font-size: 12px; font-weight: bold; font-family: 'Segoe UI', Arial;");
    card_body_label_ = new QLabel("", assistance_card_widget_);
    card_body_label_->setStyleSheet("color: #CBD5E1; font-size: 11px; font-family: 'Segoe UI', Arial;");
    card_body_label_->setWordWrap(true);
    card_action_btn_ = new QPushButton("", assistance_card_widget_);
    card_action_btn_->setStyleSheet(
        "QPushButton { color: #3B82F6; background: transparent; border: none; font-size: 11px; font-weight: bold; text-align: left; padding: 2px 0; }"
        "QPushButton:hover { color: #60A5FA; }");
    card_action_btn_->setCursor(Qt::PointingHandCursor);

    card_layout->addWidget(card_title_label_);
    card_layout->addWidget(card_body_label_);
    card_layout->addWidget(card_action_btn_);
    assistance_card_widget_->hide();

    container_layout->addWidget(assistance_card_widget_);
    container_layout->addWidget(chat_history_);
    container_layout->addLayout(input_layout);

    main_layout_->addWidget(container);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 8);
    container->setGraphicsEffect(shadow);
}

void TrayWidget::expandWidget() {
    is_expanded_ = true;
    chat_history_->show();
    setFixedSize(450, 520);
    expand_btn_->setText("-");
    expand_btn_->show();
    input_field_->setPlaceholderText("Pergunte algo...");
}

void TrayWidget::shrinkWidget() {
    is_expanded_ = false;
    chat_history_->hide();
    setFixedSize(450, 160);
    expand_btn_->hide(); // Hidden in compact mode in mockup
    input_field_->setPlaceholderText("Pergunte algo rápido...");
}

void TrayWidget::setStatusText(const QString& text) {
    status_label_->setText(text);
}

void TrayWidget::setPresenceState(PresenceState state) {
    status_label_->setText(QString("Eu Digital — %1").arg(presenceStateName(state)));
}

void TrayWidget::appendMessage(const QString& role, const QString& text) {
    if (!is_expanded_) {
        expandWidget();
        emit expandRequested();
    }
    
    QString html;
    if (role == "user") {
        html = QString("<div style='text-align: right; margin-bottom: 12px;'><span style='background-color: #3B82F6; color: white; padding: 10px 14px; border-radius: 12px; display: inline-block;'>%1</span></div><br/>").arg(text.toHtmlEscaped());
    } else {
        html = QString("<div style='text-align: left; margin-bottom: 12px;'><span style='background-color: #1E1E22; color: #E2E8F0; padding: 10px 14px; border-radius: 12px; display: inline-block;'>%1</span></div><br/>").arg(text.toHtmlEscaped());
    }
    chat_history_->append(html);
}

void TrayWidget::toggleVisibility(const QPoint& trayIconPos) {
    if (isVisible()) {
        hide();
    } else {
        QScreen* screen = QGuiApplication::screenAt(trayIconPos);
        if (!screen) screen = QGuiApplication::primaryScreen();
        
        QRect screenGeometry = screen->availableGeometry();
        
        int x = trayIconPos.x() - width() / 2;
        int y = trayIconPos.y() - height() - 10;
        
        if (x + width() > screenGeometry.right()) {
            x = screenGeometry.right() - width() - 10;
        }
        if (y < screenGeometry.top()) {
            y = trayIconPos.y() + 10;
        }
        
        move(x, y);
        show();
        activateWindow();
        input_field_->setFocus();
    }
}

void TrayWidget::onInputReturnPressed() {
    QString text = input_field_->text().trimmed();
    if (!text.isEmpty()) {
        appendMessage("user", text);
        input_field_->clear();
        emit userInputReceived(text);
    }
}

void TrayWidget::onExpandClicked() {
    if (is_expanded_) {
        shrinkWidget();
    } else {
        expandWidget();
        emit expandRequested();
    }
}

void TrayWidget::onCancelClicked() {
    emit cancelRequested();
}

void TrayWidget::onSettingsClicked() {
    emit settingsRequested();
    hide();
}

bool TrayWidget::event(QEvent* e) {
    if (e->type() == QEvent::WindowDeactivate) {
        hide();
    }
    return QWidget::event(e);
}

} // namespace eu_digital

// SPEC-053: Activity companion methods
void eu_digital::TrayWidget::setCurrentActivity(const QString& description, const QString& duration) {
    if (activity_label_) {
        activity_label_->setText(description.isEmpty() ? "Nenhuma atividade detectada" : description);
    }
    if (activity_duration_label_) {
        activity_duration_label_->setText(duration);
    }
}

void eu_digital::TrayWidget::setAssistanceCard(const QString& title, const QString& body,
                                                const QString& action_label, const QString& card_type) {
    if (!assistance_card_widget_) return;

    card_title_label_->setText(title);
    card_body_label_->setText(body);
    card_action_btn_->setText(action_label);

    // Style the card border based on type
    if (card_type == "suggestion") {
        assistance_card_widget_->setStyleSheet(
            "QWidget { background-color: #1E293B; border-radius: 8px; border: 1px solid #3B82F6; }");
    } else if (card_type == "question") {
        assistance_card_widget_->setStyleSheet(
            "QWidget { background-color: #1E293B; border-radius: 8px; border: 1px solid #F59E0B; }");
    } else {
        assistance_card_widget_->setStyleSheet(
            "QWidget { background-color: #1E293B; border-radius: 8px; border: 1px solid #334155; }");
    }

    assistance_card_widget_->show();
}

void eu_digital::TrayWidget::clearAssistanceCard() {
    if (assistance_card_widget_) {
        assistance_card_widget_->hide();
    }
}
