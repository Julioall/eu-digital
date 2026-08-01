#pragma once

#include <QWidget>
#include <QListWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "sensor_control_widget.hpp"

class QTextBrowser;

namespace eu_digital {

class SettingsWindow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

    void loadSettings();
    void saveSettings();

    void setCapabilityRegistry(const CapabilityRegistry* registry);

signals:
    void settingsChanged();
    void sensorStateChangeRequested(const QString& capability_id, bool pause);

protected:
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void setupUi();
    void createToggleStyle();
    QWidget* createGeralTab();
    QWidget* createSensoresTab();
    QWidget* createPerguntasTab();
    QWidget* createPrivacidadeTab();
    QWidget* createNotificacoesTab();
    QWidget* createAparenciaTab();
    QWidget* createDiagnosticoTab();
    QWidget* createSobreTab();

    QListWidget* sidebar_;
    QStackedWidget* stacked_content_;
    
    QCheckBox* auto_start_cb_;
    QCheckBox* continuous_cb_;
    QCheckBox* proactive_cb_;
    QCheckBox* focus_cb_;
    QComboBox* lang_combo_;
    
    QSlider* privacy_slider_;
    QTextBrowser* log_viewer_;
    SensorControlWidget* sensor_control_{nullptr};
};

} // namespace eu_digital
