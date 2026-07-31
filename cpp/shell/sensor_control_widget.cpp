#include "sensor_control_widget.hpp"
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>

namespace eu_digital {

SensorControlWidget::SensorControlWidget(QWidget* parent) : QWidget(parent) {
    main_layout_ = new QVBoxLayout(this);
    main_layout_->setContentsMargins(0, 0, 0, 0);

    QLabel* title = new QLabel("Sensores Cognitivos", this);
    title->setStyleSheet("color: white; font-size: 16px; font-weight: bold; font-family: 'Segoe UI', Arial;");
    main_layout_->addWidget(title);
    
    QLabel* desc = new QLabel("Controle a captura de dados e status dos sensores", this);
    desc->setStyleSheet("color: #A0A0A0; font-size: 12px; margin-bottom: 10px;");
    main_layout_->addWidget(desc);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");

    scroll_content_ = new QWidget(scroll);
    scroll_content_->setStyleSheet("background-color: transparent;");
    sensors_layout_ = new QVBoxLayout(scroll_content_);
    sensors_layout_->setContentsMargins(0, 0, 0, 0);
    sensors_layout_->setSpacing(8);
    sensors_layout_->addStretch();

    scroll->setWidget(scroll_content_);
    main_layout_->addWidget(scroll);
}

void SensorControlWidget::updateFromRegistry(const CapabilityRegistry* registry) {
    // Clear old items (except the stretch at the end)
    while (QLayoutItem* item = sensors_layout_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (!registry) {
        sensors_layout_->addStretch();
        return;
    }

    const auto& records = registry->records();
    for (const auto& [id, record] : records) {
        // We only care about sensors (or we can show all ports, but let's show all for debugging/control)
        QFrame* card = new QFrame(scroll_content_);
        card->setStyleSheet("QFrame { background-color: #1A1A2E; border-radius: 8px; border: 1px solid #232328; }");
        
        QHBoxLayout* card_layout = new QHBoxLayout(card);
        
        QVBoxLayout* text_layout = new QVBoxLayout();
        QLabel* name_label = new QLabel(QString::fromStdString(record.descriptor.capability_id), card);
        name_label->setStyleSheet("color: white; font-weight: bold; font-size: 13px;");
        
        QString state_str;
        QString state_color;
        switch (record.state.state) {
            case CapabilityState::available: state_str = "Ativo"; state_color = "#10B981"; break;
            case CapabilityState::disabled: state_str = "Desativado"; state_color = "#9CA3AF"; break;
            case CapabilityState::failed: state_str = "Falha"; state_color = "#EF4444"; break;
            case CapabilityState::degraded: state_str = "Degradado"; state_color = "#F59E0B"; break;
            case CapabilityState::temporarily_unavailable: state_str = "Pausado"; state_color = "#F59E0B"; break;
            default: state_str = "Desconhecido"; state_color = "#9CA3AF"; break;
        }

        QLabel* status_label = new QLabel(QString("Status: <span style='color:%1;'>%2</span>").arg(state_color, state_str), card);
        status_label->setStyleSheet("color: #A0A0A0; font-size: 11px;");
        
        text_layout->addWidget(name_label);
        text_layout->addWidget(status_label);
        
        QPushButton* toggle_btn = new QPushButton(record.state.state == CapabilityState::available ? "Pausar" : "Ativar", card);
        toggle_btn->setStyleSheet(
            "QPushButton { background-color: #2A2A2E; color: white; padding: 4px 12px; border-radius: 4px; font-size: 11px; }"
            "QPushButton:hover { background-color: #3A3A3E; }"
        );
        // Note: actual toggling requires mutating the registry. This is just UI for Phase 6.

        card_layout->addLayout(text_layout);
        card_layout->addStretch();
        card_layout->addWidget(toggle_btn);
        
        sensors_layout_->insertWidget(sensors_layout_->count(), card);
    }
    
    sensors_layout_->addStretch();
}

} // namespace eu_digital
