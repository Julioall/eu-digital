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

class QTextBrowser;

namespace eu_digital {

class SettingsWindow : public QWidget {
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();

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
};

} // namespace eu_digital
