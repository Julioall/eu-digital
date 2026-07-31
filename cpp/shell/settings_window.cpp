#include "settings_window.hpp"
#include <QSettings>
#include <QLabel>
#include <QCloseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTextBrowser>
#include "log_manager.hpp"

namespace eu_digital {

SettingsWindow::SettingsWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(600, 400);
    
    setupUi();
    loadSettings();
}

void SettingsWindow::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor("#121215"));
    p.setPen(QPen(QColor("#232328"), 1));
    p.drawRoundedRect(rect().adjusted(1,1,-1,-1), 12, 12);
}

void SettingsWindow::setupUi() {
    QHBoxLayout* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    
    // Sidebar
    QWidget* sidebar_container = new QWidget(this);
    sidebar_container->setFixedWidth(180);
    sidebar_container->setStyleSheet("background-color: #1A1A1E; border-top-left-radius: 12px; border-bottom-left-radius: 12px; border-right: 1px solid #232328;");
    
    QVBoxLayout* sidebar_layout = new QVBoxLayout(sidebar_container);
    sidebar_layout->setContentsMargins(8, 24, 8, 24);
    
    sidebar_ = new QListWidget(sidebar_container);
    sidebar_->setStyleSheet(
        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { color: #A0A0A0; padding: 10px 14px; border-radius: 8px; margin-bottom: 4px; font-family: 'Segoe UI', Arial; font-size: 13px; }"
        "QListWidget::item:selected { background-color: #252529; color: #3B82F6; font-weight: bold; }"
        "QListWidget::item:hover:!selected { background-color: #1E1E22; }"
    );
    
    QListWidgetItem* item_geral = new QListWidgetItem(QString::fromUtf8("\xE2\x8C\x82  Geral"));
    QListWidgetItem* item_sensors = new QListWidgetItem(QString::fromUtf8("\xE2\x8A\x9E  Sensores"));
    QListWidgetItem* item_perguntas = new QListWidgetItem(QString::fromUtf8("?  Perguntas"));
    QListWidgetItem* item_priv = new QListWidgetItem(QString::fromUtf8("\xE2\x9A\xBF  Privacidade"));
    QListWidgetItem* item_notif = new QListWidgetItem(QString::fromUtf8("\xF0\x9F\x94\x94  Notificações"));
    QListWidgetItem* item_aparencia = new QListWidgetItem(QString::fromUtf8("\xE2\x9C\xA8  Aparência"));
    QListWidgetItem* item_diag = new QListWidgetItem(QString::fromUtf8("\xE2\x9B\xA8  Diagnóstico"));
    QListWidgetItem* item_sobre = new QListWidgetItem(QString::fromUtf8("\xE2\x84\xB9  Sobre"));
    
    sidebar_->addItem(item_geral);
    sidebar_->addItem(item_sensors);
    sidebar_->addItem(item_perguntas);
    sidebar_->addItem(item_priv);
    sidebar_->addItem(item_notif);
    sidebar_->addItem(item_aparencia);
    sidebar_->addItem(item_diag);
    sidebar_->addItem(item_sobre);
    
    sidebar_layout->addWidget(sidebar_);
    
    // Content Area
    QWidget* content_container = new QWidget(this);
    QVBoxLayout* content_layout = new QVBoxLayout(content_container);
    content_layout->setContentsMargins(32, 24, 32, 24);
    
    QHBoxLayout* top_bar = new QHBoxLayout();
    QLabel* current_tab_lbl = new QLabel("Geral", content_container);
    current_tab_lbl->setStyleSheet("color: white; font-weight: bold; font-size: 16px; font-family: 'Segoe UI', Arial;");
    
    QPushButton* close_btn = new QPushButton(QString::fromUtf8("\xE2\x95\xB3"), content_container);
    close_btn->setFixedSize(24, 24);
    close_btn->setStyleSheet("QPushButton { background: transparent; color: #A0A0A0; border: none; font-size: 14px; } QPushButton:hover { color: white; }");
    connect(close_btn, &QPushButton::clicked, this, [this]() {
        saveSettings();
        hide();
        emit settingsChanged();
    });
    
    QPushButton* minimize_btn = new QPushButton(QString::fromUtf8("\xE2\x80\x93"), content_container);
    minimize_btn->setFixedSize(24, 24);
    minimize_btn->setStyleSheet("QPushButton { background: transparent; color: #A0A0A0; border: none; font-size: 14px; } QPushButton:hover { color: white; }");
    connect(minimize_btn, &QPushButton::clicked, this, &SettingsWindow::hide);
    
    top_bar->addWidget(current_tab_lbl);
    top_bar->addStretch();
    top_bar->addWidget(minimize_btn);
    top_bar->addWidget(close_btn);
    
    stacked_content_ = new QStackedWidget(content_container);
    stacked_content_->addWidget(createGeralTab());
    stacked_content_->addWidget(createSensoresTab());
    stacked_content_->addWidget(createPerguntasTab());
    stacked_content_->addWidget(createPrivacidadeTab());
    stacked_content_->addWidget(createNotificacoesTab());
    stacked_content_->addWidget(createAparenciaTab());
    stacked_content_->addWidget(createDiagnosticoTab());
    stacked_content_->addWidget(createSobreTab());
    
    content_layout->addLayout(top_bar);
    content_layout->addSpacing(20);
    content_layout->addWidget(stacked_content_);
    
    main_layout->addWidget(sidebar_container);
    main_layout->addWidget(content_container);
    
    connect(sidebar_, &QListWidget::currentRowChanged, this, [this, current_tab_lbl](int row) {
        if (row < stacked_content_->count()) {
            stacked_content_->setCurrentIndex(row);
        }
        current_tab_lbl->setText(sidebar_->item(row)->text().remove(QRegularExpression("^[^a-zA-Z]+"))); // Limpa o ícone
    });
    
    sidebar_->setCurrentRow(0);
}

// Helpers for iOS style toggles
QWidget* createToggleRow(const QString& title, const QString& desc, QCheckBox*& cb, QWidget* parent) {
    QWidget* row = new QWidget(parent);
    QHBoxLayout* l = new QHBoxLayout(row);
    l->setContentsMargins(0, 8, 0, 8);
    
    QVBoxLayout* tl = new QVBoxLayout();
    tl->setSpacing(2);
    QLabel* t = new QLabel(title, row);
    t->setStyleSheet("color: white; font-size: 13px; font-family: 'Segoe UI', Arial;");
    QLabel* d = new QLabel(desc, row);
    d->setStyleSheet("color: #A0A0A0; font-size: 11px; font-family: 'Segoe UI', Arial;");
    tl->addWidget(t);
    tl->addWidget(d);
    
    cb = new QCheckBox(row);
    // iOS Toggle Switch styling
    cb->setStyleSheet(
        "QCheckBox { spacing: 0px; }"
        "QCheckBox::indicator { width: 40px; height: 22px; border-radius: 11px; }"
        "QCheckBox::indicator:unchecked { background-color: #252529; border: 1px solid #3A3A3E; }"
        "QCheckBox::indicator:unchecked:hover { background-color: #2A2A2E; }"
        "QCheckBox::indicator:checked { background-color: #3B82F6; border: 1px solid #3B82F6; }"
        // A bolinha precisa de um ícone customizado se não puder usar qradialgradient, 
        // mas faremos um simples quadrado arredondado cinza/branco como trick de image no QSS
        // Por praticidade usaremos cores solídas que já parecem switch.
    );
    
    l->addLayout(tl);
    l->addStretch();
    l->addWidget(cb);
    return row;
}

QWidget* SettingsWindow::createGeralTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    l->setSpacing(8);
    
    l->addWidget(createToggleRow("Iniciar com o Windows", "Abrir o Eu Digital ao ligar o computador", auto_start_cb_, tab));
    l->addWidget(createToggleRow("Monitoramento contínuo", "Manter sensores ativos em segundo plano", continuous_cb_, tab));
    l->addWidget(createToggleRow("Perguntas proativas", "Permitir que o Eu Digital faça perguntas", proactive_cb_, tab));
    l->addWidget(createToggleRow("Modo foco automático", "Reduzir interrupções em momentos de foco", focus_cb_, tab));
    
    l->addSpacing(16);
    QLabel* lbl = new QLabel("Idioma", tab);
    lbl->setStyleSheet("color: white; font-size: 13px; font-family: 'Segoe UI', Arial;");
    l->addWidget(lbl);
    
    lang_combo_ = new QComboBox(tab);
    lang_combo_->addItems({"Português (Brasil)", "English"});
    lang_combo_->setStyleSheet(
        "QComboBox { background: #1A1A1E; color: white; border: 1px solid #2A2A2E; border-radius: 6px; padding: 6px 12px; font-size: 13px; }"
        "QComboBox::drop-down { border: none; }"
    );
    l->addWidget(lang_combo_);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createPrivacidadeTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    
    QLabel* priv_lbl = new QLabel("Nível de Privacidade", tab);
    priv_lbl->setStyleSheet("color: white; font-size: 14px; font-weight: bold; font-family: 'Segoe UI', Arial;");
    privacy_slider_ = new QSlider(Qt::Horizontal, tab);
    privacy_slider_->setRange(1, 3);
    privacy_slider_->setStyleSheet(
        "QSlider::groove:horizontal { border-radius: 2px; height: 4px; background: #2A2A2E; }"
        "QSlider::handle:horizontal { background: #3B82F6; width: 16px; margin: -6px 0; border-radius: 8px; }"
    );
    
    QLabel* priv_desc = new QLabel("1 = Máxima (Sem longo prazo)\n3 = Flexível (Longo prazo ativo)", tab);
    priv_desc->setStyleSheet("color: #A0A0A0; font-size: 12px;");
    
    l->addWidget(priv_lbl);
    l->addSpacing(16);
    l->addWidget(privacy_slider_);
    l->addWidget(priv_desc);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createSensoresTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    QLabel* lbl = new QLabel("Configurações de Sensores (WIP)", tab);
    lbl->setStyleSheet("color: #A0A0A0;");
    l->addWidget(lbl);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createPerguntasTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    QLabel* lbl = new QLabel("Regras de Interação (WIP)", tab);
    lbl->setStyleSheet("color: #A0A0A0;");
    l->addWidget(lbl);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createNotificacoesTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    QLabel* lbl = new QLabel("Frequência de Notificações (WIP)", tab);
    lbl->setStyleSheet("color: #A0A0A0;");
    l->addWidget(lbl);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createAparenciaTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    QLabel* lbl = new QLabel("Temas e Cores (WIP)", tab);
    lbl->setStyleSheet("color: #A0A0A0;");
    l->addWidget(lbl);
    l->addStretch();
    return tab;
}

QWidget* SettingsWindow::createDiagnosticoTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    
    QLabel* lbl = new QLabel("Logs do Sistema", tab);
    lbl->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
    l->addWidget(lbl);
    
    log_viewer_ = new QTextBrowser(tab);
    log_viewer_->setStyleSheet("QTextBrowser { background-color: #121215; border: 1px solid #232328; border-radius: 6px; padding: 4px; font-family: Consolas, monospace; font-size: 11px; }");
    l->addWidget(log_viewer_);
    
    QHBoxLayout* btn_l = new QHBoxLayout();
    QPushButton* clear_btn = new QPushButton("Limpar", tab);
    clear_btn->setStyleSheet("QPushButton { background-color: #2A2A2E; color: white; padding: 4px 12px; border-radius: 4px; } QPushButton:hover { background-color: #3A3A3E; }");
    connect(clear_btn, &QPushButton::clicked, this, [this]() {
        LogManager::instance().clearLogs();
        log_viewer_->clear();
    });
    btn_l->addStretch();
    btn_l->addWidget(clear_btn);
    l->addLayout(btn_l);

    // Initial load
    auto logs = LogManager::instance().getRecentLogs();
    for (const auto& entry : logs) {
        QString color = "white";
        if (entry.level == "DEBUG") color = "#94A3B8";
        else if (entry.level == "INFO") color = "#3B82F6";
        else if (entry.level == "WARN") color = "#F59E0B";
        else if (entry.level == "CRITICAL" || entry.level == "FATAL") color = "#EF4444";
        
        QString html = QString("<span style='color: #64748B;'>[%1]</span> <span style='color: %2; font-weight: bold;'>%3:</span> <span style='color: #E2E8F0;'>%4</span>")
            .arg(entry.timestamp).arg(color).arg(entry.level).arg(entry.message.toHtmlEscaped());
        log_viewer_->append(html);
    }
    
    connect(&LogManager::instance(), &LogManager::newLogEntry, this, [this](const QString& html) {
        log_viewer_->append(html);
    });

    return tab;
}

QWidget* SettingsWindow::createSobreTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* l = new QVBoxLayout(tab);
    l->setContentsMargins(0,0,0,0);
    QLabel* title = new QLabel("Eu Digital - Local Agent", tab);
    title->setStyleSheet("color: white; font-weight: bold; font-size: 16px;");
    QLabel* ver = new QLabel("Versão 1.0.0-beta", tab);
    ver->setStyleSheet("color: #A0A0A0;");
    l->addWidget(title);
    l->addWidget(ver);
    l->addStretch();
    return tab;
}

void SettingsWindow::loadSettings() {
    QSettings settings("EU-Digital", "DesktopRuntime");
    auto_start_cb_->setChecked(settings.value("auto_start", true).toBool());
    proactive_cb_->setChecked(settings.value("notifications_enabled", true).toBool());
    continuous_cb_->setChecked(true); // mock
    focus_cb_->setChecked(false); // mock
    privacy_slider_->setValue(settings.value("privacy_level", 2).toInt());
}

void SettingsWindow::saveSettings() {
    QSettings settings("EU-Digital", "DesktopRuntime");
    settings.setValue("auto_start", auto_start_cb_->isChecked());
    settings.setValue("notifications_enabled", proactive_cb_->isChecked());
    settings.setValue("privacy_level", privacy_slider_->value());
}

void SettingsWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}

} // namespace eu_digital
