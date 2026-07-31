#pragma once

#include "core/procedural_avatar.hpp"
#include <QQuickView>
#include <QObject>
#include <QTimer>

namespace eu_digital {

/// Qt 6 / QML Shell Adapter for the Procedural Avatar (SPEC-042).
/// Does not execute actions, does not capture focus, respects global pause/consent.
class QtAvatarWindow : public QQuickView {
    Q_OBJECT
public:
    explicit QtAvatarWindow(std::shared_ptr<ProceduralAvatarRenderer> renderer, QWindow* parent = nullptr);
    ~QtAvatarWindow() override;

    bool blocks_work() const { return false; }
    bool captures_input() const { return false; }

public slots:
    void updateFrame();

private:
    std::shared_ptr<ProceduralAvatarRenderer> renderer_;
    QTimer* frame_timer_;
};

}  // namespace eu_digital
