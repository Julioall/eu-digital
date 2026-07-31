#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include "core/capability_runtime.hpp"

namespace eu_digital {

class SensorControlWidget : public QWidget {
    Q_OBJECT
public:
    explicit SensorControlWidget(QWidget* parent = nullptr);

    // Updates the widget with the current capability registry.
    // In a real implementation, we would listen to events.
    void updateFromRegistry(const CapabilityRegistry* registry);

private:
    QVBoxLayout* main_layout_;
    QWidget* scroll_content_;
    QVBoxLayout* sensors_layout_;
};

} // namespace eu_digital
