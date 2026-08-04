#include "shell/qt_avatar_window.hpp"
#include <QEvent>
#include <QGuiApplication>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eu_digital {
namespace {

void applyNativeNonActivatingStyle(QWindow* window) {
#ifdef _WIN32
    const auto handle = reinterpret_cast<HWND>(window->winId());
    if (handle == nullptr) return;
    const LONG_PTR current = GetWindowLongPtrW(handle, GWL_EXSTYLE);
    SetWindowLongPtrW(handle, GWL_EXSTYLE,
                      current | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE |
                     SWP_NOZORDER | SWP_NOACTIVATE);
#else
    (void)window;
#endif
}

}  // namespace

QtAvatarWindow::QtAvatarWindow(std::shared_ptr<ProceduralAvatarRenderer> renderer, QWindow* parent)
    : QQuickView(parent), renderer_(std::move(renderer)) {
    
    // Transparent background, click-through, always on top
    setColor(Qt::transparent);
    setFlags(Qt::WindowTransparentForInput | Qt::WindowDoesNotAcceptFocus |
             Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint | Qt::Tool);
    
    frame_timer_ = new QTimer(this);
    connect(frame_timer_, &QTimer::timeout, this, &QtAvatarWindow::updateFrame);
    frame_timer_->start(16); // ~60fps target
}

QtAvatarWindow::~QtAvatarWindow() = default;

bool QtAvatarWindow::event(QEvent* event) {
    const bool handled = QQuickView::event(event);
    if (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange) {
        applyNativeNonActivatingStyle(this);
    }
    return handled;
}

void QtAvatarWindow::updateFrame() {
    if (!renderer_) return;
    
    // In a real implementation, we would extract the frame from the renderer
    // auto frame = renderer_->render("2026-07-31T12:00:00+00:00");
    // And push it to the QML rendering context.
}

}  // namespace eu_digital
