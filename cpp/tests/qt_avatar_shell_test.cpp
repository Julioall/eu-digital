#include "shell/qt_avatar_window.hpp"
#include "shell/qt_tray_adapter.hpp"

#include <QAccessible>
#include <QApplication>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScreen>
#include <QSystemTrayIcon>

#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    const bool native_windows = QGuiApplication::platformName() == "windows";
    const bool require_native_windows =
        app.arguments().contains("--require-native-windows");
    assert(!require_native_windows || native_windows);
    std::optional<double> expected_scale;
    const QString scale_prefix = "--expected-scale=";
    for (const auto& argument : app.arguments()) {
        if (argument.startsWith(scale_prefix)) {
            expected_scale = argument.mid(scale_prefix.size()).toDouble();
        }
    }

    assert(!window.blocks_work());
    assert(!window.captures_input());
    assert(window.flags().testFlag(Qt::WindowTransparentForInput));
    assert(window.flags().testFlag(Qt::WindowDoesNotAcceptFocus));
    assert(window.flags().testFlag(Qt::FramelessWindowHint));
    assert(window.flags().testFlag(Qt::WindowStaysOnTopHint));
    assert(window.color().alpha() == 0);
    assert(!QGuiApplication::screens().isEmpty());

    QJsonArray screens;
    for (const auto* screen : QGuiApplication::screens()) {
        assert(screen != nullptr);
        assert(screen->devicePixelRatio() > 0.0);
        assert(screen->logicalDotsPerInch() > 0.0);
        screens.append(QJsonObject{
            {"name", screen->name()},
            {"device_pixel_ratio", screen->devicePixelRatio()},
            {"logical_dpi", screen->logicalDotsPerInch()},
            {"physical_dpi", screen->physicalDotsPerInch()},
            {"width", screen->geometry().width()},
            {"height", screen->geometry().height()},
        });
    }
    if (expected_scale.has_value()) {
        assert(std::abs(QGuiApplication::primaryScreen()->devicePixelRatio() -
                        *expected_scale) < 0.01);
    }

    bool native_click_through = false;
    bool native_no_activate = false;
    if (native_windows) {
        const auto available =
            QGuiApplication::primaryScreen()->availableGeometry();
        window.setGeometry(available.x() + 20, available.y() + 20, 64, 64);
        window.show();
        QCoreApplication::processEvents();
#ifdef _WIN32
        const auto handle = reinterpret_cast<HWND>(window.winId());
        assert(handle != nullptr);
        const LONG_PTR extended_style = GetWindowLongPtrW(handle, GWL_EXSTYLE);
        native_click_through = (extended_style & WS_EX_TRANSPARENT) != 0;
        native_no_activate = (extended_style & WS_EX_NOACTIVATE) != 0;
        assert(native_click_through);
        assert(native_no_activate);
#endif
        window.hide();
    }

    QtTrayAdapter tray;
    auto* tray_widget = tray.getTrayWidget();
    assert(tray_widget != nullptr);
    assert(QAccessible::queryAccessibleInterface(tray_widget) != nullptr);
    if (native_windows) {
        assert(QSystemTrayIcon::isSystemTrayAvailable());
        tray.show();
        QCoreApplication::processEvents();
        const auto* native_tray = tray.findChild<QSystemTrayIcon*>();
        assert(native_tray != nullptr);
        assert(native_tray->isVisible());
    }

    tray.activateAt(QPoint(100, 100));
    QCoreApplication::processEvents();
    auto* input = tray_widget->findChild<QLineEdit*>("messageInput");
    assert(input != nullptr);
    assert(input->hasFocus());
    assert(input->testAttribute(Qt::WA_InputMethodEnabled));

    auto* input_accessible = QAccessible::queryAccessibleInterface(input);
    assert(input_accessible != nullptr);
    assert(input_accessible->role() == QAccessible::EditableText);
    assert(input_accessible->text(QAccessible::Name) ==
           "Mensagem para o Eu Digital");

    const QString unicode_sample =
        QString::fromUtf8("ação, informação e João");
    QInputMethodEvent composition;
    composition.setCommitString(unicode_sample);
    QApplication::sendEvent(input, &composition);
    assert(input->text() == unicode_sample);

    QString submitted;
    QObject::connect(tray_widget, &TrayWidget::userInputReceived,
                     [&submitted](const QString& value) { submitted = value; });
    QKeyEvent return_press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(input, &return_press);
    assert(submitted == unicode_sample);

    input->setFocus();
    QKeyEvent tab_press(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(input, &tab_press);
    QCoreApplication::processEvents();
    const bool keyboard_navigation = QApplication::focusWidget() != input;
    assert(keyboard_navigation);

    tray.activateAt(QPoint(100, 100));

    const QJsonObject report{
        {"report_version", "1.0"},
        {"platform_plugin", QGuiApplication::platformName()},
        {"native_windows", native_windows},
        {"screen_count", QGuiApplication::screens().size()},
        {"expected_scale", expected_scale.has_value()
                               ? QJsonValue(*expected_scale)
                               : QJsonValue(QJsonValue::Null)},
        {"screens", screens},
        {"native_click_through", native_click_through},
        {"native_no_activate", native_no_activate},
        {"system_tray_available", QSystemTrayIcon::isSystemTrayAvailable()},
        {"accessible_editable_text", true},
        {"unicode_input_method_commit", true},
        {"keyboard_navigation", keyboard_navigation},
    };
    const QString report_prefix = "--report=";
    for (const auto& argument : app.arguments()) {
        if (!argument.startsWith(report_prefix)) {
            continue;
        }
        QFile report_file(argument.mid(report_prefix.size()));
        assert(report_file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        assert(report_file.write(
                   QJsonDocument(report).toJson(QJsonDocument::Indented)) > 0);
    }

    std::cout << "Qt avatar shell platform invariants passed; screens="
              << QGuiApplication::screens().size()
              << "; platform="
              << QGuiApplication::platformName().toStdString() << '\n';
    return 0;
}
