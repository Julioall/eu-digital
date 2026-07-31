#include "quick_panel_widget.hpp"
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>

namespace eu_digital {

// Helper Icon for Cards
class CardIcon : public QWidget {
public:
    CardIcon(const QString& glyph, const QString& colorHex, QWidget* parent = nullptr) : QWidget(parent), glyph_(glyph), color_(colorHex) {
        setFixedSize(40, 40);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        
        QColor baseColor(color_);
        baseColor.setAlpha(40); // 15% opacity background
        p.setBrush(baseColor);
        p.drawEllipse(0, 0, 40, 40);
        
        p.setPen(QColor(color_));
        p.setFont(QFont("Segoe UI Symbol", 16));
        p.drawText(rect(), Qt::AlignCenter, glyph_);
    }
private:
    QString glyph_;
    QString color_;
};

QuickPanelWidget::QuickPanelWidget(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(400, 280);
    
    setupUi();
}

QWidget* QuickPanelWidget::createCard(const QString& title, const QString& mainText, const QString& subText, const QString& iconGlyph, const QString& iconColor, QLabel*& mainRef, QLabel*& subRef) {
    QWidget* card = new QWidget(this);
    card->setStyleSheet(
        "QWidget { "
        "   background-color: #1E1E22; "
        "   border-radius: 12px; "
        "}"
    );
    
    QHBoxLayout* layout = new QHBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    
    CardIcon* icon = new CardIcon(iconGlyph, iconColor, card);
    layout->addWidget(icon);
    
    QVBoxLayout* text_layout = new QVBoxLayout();
    text_layout->setSpacing(2);
    QLabel* title_lbl = new QLabel(title, card);
    title_lbl->setStyleSheet("color: white; font-weight: bold; font-size: 13px; font-family: 'Segoe UI', Arial;");
    
    mainRef = new QLabel(mainText, card);
    mainRef->setStyleSheet("color: #A0A0A0; font-size: 12px;");
    
    subRef = new QLabel(subText, card);
    subRef->setStyleSheet("color: #10B981; font-size: 11px;"); // Default green
    
    text_layout->addWidget(title_lbl);
    text_layout->addWidget(mainRef);
    text_layout->addWidget(subRef);
    
    layout->addLayout(text_layout);
    layout->addStretch();
    
    QLabel* arrow = new QLabel(">", card);
    arrow->setStyleSheet("color: #555555; font-weight: bold; font-size: 14px;");
    layout->addWidget(arrow);
    
    return card;
}

void QuickPanelWidget::setupUi() {
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    
    QWidget* container = new QWidget(this);
    container->setStyleSheet(
        "QWidget { "
        "   background-color: #121215; "
        "   border-radius: 16px; "
        "   border: 1px solid #232328; "
        "}"
    );

    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    QLabel* title = new QLabel("PAINEL RÁPIDO", container);
    title->setStyleSheet("color: #6B7280; font-size: 11px; font-weight: bold; letter-spacing: 1px;");
    layout->addWidget(title);
    
    QGridLayout* grid = new QGridLayout();
    grid->setSpacing(12);
    
    grid->addWidget(createCard("Sensores", "0 ativos", "Pausados", QString::fromUtf8("\xE2\x86\x94"), "#3B82F6", sensors_main_, sensors_sub_), 0, 0);
    grid->addWidget(createCard("Perguntas Hoje", "0", "+0 que ontem", QString::fromUtf8("?"), "#3B82F6", questions_main_, questions_sub_), 0, 1);
    grid->addWidget(createCard("Memórias", "0", "Nenhuma recente", QString::fromUtf8("\xE2\x96\xA4"), "#8B5CF6", memories_main_, memories_sub_), 1, 0);
    grid->addWidget(createCard("Saúde do Sistema", "Aguardando...", "-", QString::fromUtf8("\xE2\x9B\xA8"), "#10B981", health_main_, health_sub_), 1, 1);

    layout->addLayout(grid);
    layout->addStretch();
    
    QPushButton* link_btn = new QPushButton(QString::fromUtf8("Abrir painel completo \xE2\x86\x92"), container);
    link_btn->setStyleSheet("QPushButton { color: #3B82F6; background: transparent; border: none; font-size: 12px; font-weight: bold; } QPushButton:hover { color: #60A5FA; text-decoration: underline; }");
    link_btn->setCursor(Qt::PointingHandCursor);
    layout->addWidget(link_btn, 0, Qt::AlignCenter);
    
    main_layout->addWidget(container);

    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 8);
    container->setGraphicsEffect(shadow);
}

void QuickPanelWidget::updateHealthStats(int sensors_active, int total_memories, bool is_paused) {
    current_pause_state_ = is_paused;
    sensors_main_->setText(QString("%1 ativos").arg(sensors_active));
    memories_main_->setText(QString::number(total_memories));
    
    if (is_paused) {
        sensors_sub_->setText("Pausados");
        sensors_sub_->setStyleSheet("color: #F59E0B; font-size: 11px;");
        health_main_->setText("Pausado");
        health_sub_->setText("Operação interrompida");
        health_sub_->setStyleSheet("color: #F59E0B; font-size: 11px;");
    } else {
        sensors_sub_->setText("Todos operando");
        sensors_sub_->setStyleSheet("color: #10B981; font-size: 11px;");
        health_main_->setText("Excelente");
        health_sub_->setText("Tudo funcionando bem");
        health_sub_->setStyleSheet("color: #10B981; font-size: 11px;");
    }
}

bool QuickPanelWidget::event(QEvent* e) {
    if (e->type() == QEvent::WindowDeactivate) {
        hide();
    }
    return QWidget::event(e);
}

} // namespace eu_digital
