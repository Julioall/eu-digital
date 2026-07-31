#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <vector>

namespace eu_digital {

struct LogEntry {
    QString timestamp;
    QString level;
    QString message;
};

class LogManager : public QObject {
    Q_OBJECT
public:
    static LogManager& instance();

    // Habilita a interceptação de logs usando qInstallMessageHandler
    void install();
    void uninstall();

    std::vector<LogEntry> getRecentLogs() const;
    void clearLogs();

    // The handler function compatible with QtMessageHandler
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);

signals:
    void newLogEntry(const QString& entryHtml);

private:
    LogManager(QObject* parent = nullptr);
    ~LogManager();

    Q_DISABLE_COPY(LogManager)

    void appendLog(const LogEntry& entry);

    std::vector<LogEntry> logs_;
    mutable QMutex mutex_;
    int max_logs_{1000};
};

} // namespace eu_digital
