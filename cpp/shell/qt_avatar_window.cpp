#include "shell/qt_avatar_window.hpp"
#include <QGuiApplication>

namespace eu_digital {

QtAvatarWindow::QtAvatarWindow(std::shared_ptr<ProceduralAvatarRenderer> renderer, QWindow* parent)
    : QQuickView(parent), renderer_(std::move(renderer)) {
    
    // Transparent background, click-through, always on top
    setColor(Qt::transparent);
    setFlags(Qt::WindowTransparentForInput | Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
    
    frame_timer_ = new QTimer(this);
    connect(frame_timer_, &QTimer::timeout, this, &QtAvatarWindow::updateFrame);
    frame_timer_->start(16); // ~60fps target
}

QtAvatarWindow::~QtAvatarWindow() = default;

void QtAvatarWindow::updateFrame() {
    if (!renderer_) return;
    
    // In a real implementation, we would extract the frame from the renderer
    // auto frame = renderer_->render("2026-07-31T12:00:00+00:00");
    // And push it to the QML rendering context.
}

}  // namespace eu_digital
