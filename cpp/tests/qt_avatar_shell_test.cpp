#include "shell/qt_avatar_window.hpp"
#include "shell/qt_tray_adapter.hpp"

#include <QAccessible>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

#include <cassert>
#include <iostream>
#include <memory>

using namespace eu_digital;

int main(int argc, char** argv) {
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);

    AvatarViewState view_state;
    view_state.view_id = "qt-shell-test";
    auto renderer = std::make_shared<ProceduralAvatarRenderer>(
        AvatarPresentationProfile{}, view_state);
    QtAvatarWindow window(renderer);

    assert(!window.blocks_work());
    assert(!window.captures_input());
    assert(window.flags().testFlag(Qt::WindowTransparentForInput));
    assert(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
    assert(window.flags().testFlag(Qt::FramelessWindowHint));
    assert(window.flags().testFlag(Qt::WindowStaysOnTopHint));
    assert(window.color().alpha() == 0);
    assert(!QGuiApplication::screens().isEmpty());
    for (const auto* screen : QGuiApplication::screens()) {
        assert(screen != nullptr);
        assert(screen->devicePixelRatio() > 0.0);
        assert(screen->logicalDotsPerInch() > 0.0);
    }

    QtTrayAdapter tray;
    assert(tray.getTrayWidget() != nullptr);
    assert(QAccessible::queryAccessibleInterface(tray.getTrayWidget()) != nullptr);
    tray.activateAt(QPoint(100, 100));
    QCoreApplication::processEvents();
    tray.activateAt(QPoint(100, 100));

    std::cout << "Qt avatar shell platform invariants passed; screens="
              << QGuiApplication::screens().size() << '\n';
    return 0;
}
