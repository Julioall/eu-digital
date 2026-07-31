#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

namespace eu_digital {

class QuickPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit QuickPanelWidget(QWidget* parent = nullptr);

    void updateHealthStats(int sensors_active, int total_memories, bool is_paused);

signals:
    void pauseRequested(bool pause);

protected:
    bool event(QEvent* e) override;

private:
    void setupUi();
    QWidget* createCard(const QString& title, const QString& mainText, const QString& subText, const QString& iconGlyph, const QString& iconColor, QLabel*& mainRef, QLabel*& subRef);

    QLabel* sensors_main_;
    QLabel* sensors_sub_;
    QLabel* questions_main_;
    QLabel* questions_sub_;
    QLabel* memories_main_;
    QLabel* memories_sub_;
    QLabel* health_main_;
    QLabel* health_sub_;

    bool current_pause_state_{false};
};

} // namespace eu_digital
