#include "log_manager.hpp"
#include <QDateTime>
#include <iostream>

namespace eu_digital {

// Variável estática para guardar o message handler original
static QtMessageHandler g_default_msg_handler = nullptr;

LogManager& LogManager::instance() {
    static LogManager instance;
    return instance;
}

LogManager::LogManager(QObject* parent) : QObject(parent) {
}

LogManager::~LogManager() {
    uninstall();
}

void LogManager::install() {
    if (!g_default_msg_handler) {
        g_default_msg_handler = qInstallMessageHandler(LogManager::messageHandler);
    }
}

void LogManager::uninstall() {
    if (g_default_msg_handler) {
        qInstallMessageHandler(g_default_msg_handler);
        g_default_msg_handler = nullptr;
    }
}

std::vector<LogEntry> LogManager::getRecentLogs() const {
    QMutexLocker locker(&mutex_);
    return logs_;
}

void LogManager::clearLogs() {
    QMutexLocker locker(&mutex_);
    logs_.clear();
}

void LogManager::appendLog(const LogEntry& entry) {
    QString htmlLine;
    QString color;

    if (entry.level == "DEBUG") color = "#94A3B8"; // Gray
    else if (entry.level == "INFO") color = "#3B82F6"; // Blue
    else if (entry.level == "WARN") color = "#F59E0B"; // Yellow
    else if (entry.level == "CRITICAL" || entry.level == "FATAL") color = "#EF4444"; // Red
    else color = "white";

    htmlLine = QString("<span style='color: #64748B;'>[%1]</span> <span style='color: %2; font-weight: bold;'>%3:</span> <span style='color: #E2E8F0;'>%4</span>")
                    .arg(entry.timestamp)
                    .arg(color)
                    .arg(entry.level)
                    .arg(entry.message.toHtmlEscaped());

    {
        QMutexLocker locker(&mutex_);
        logs_.push_back(entry);
        if (logs_.size() > max_logs_) {
            logs_.erase(logs_.begin());
        }
    }
    
    emit newLogEntry(htmlLine);
}

void LogManager::messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    // 1. Enviar para a tela (stdout/stderr) se necessário (como o Qt faz por padrão)
    if (g_default_msg_handler) {
        g_default_msg_handler(type, context, msg);
    }

    // 2. Interceptar para nossa UI
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    entry.message = msg;
    
    switch (type) {
        case QtDebugMsg:    entry.level = "DEBUG"; break;
        case QtInfoMsg:     entry.level = "INFO"; break;
        case QtWarningMsg:  entry.level = "WARN"; break;
        case QtCriticalMsg: entry.level = "CRITICAL"; break;
        case QtFatalMsg:    entry.level = "FATAL"; break;
    }

    LogManager::instance().appendLog(entry);
}

} // namespace eu_digital
