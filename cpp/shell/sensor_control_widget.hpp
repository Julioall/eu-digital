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
    void updateFromRegistry(const CapabilityRegistry* registry);

signals:
    void sensorStateChangeRequested(const QString& capability_id, bool pause);

private:
    QVBoxLayout* main_layout_;
    QWidget* scroll_content_;
    QVBoxLayout* sensors_layout_;
};

} // namespace eu_digital
