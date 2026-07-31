#include "shell/desktop_controller.hpp"
#include "shell/log_manager.hpp"

#include <QApplication>
#include <QSurfaceFormat>
#include <QDir>
#include <iostream>

using namespace eu_digital;

int main(int argc, char* argv[]) {
    LogManager::instance().install();

    QApplication app(argc, argv);
    app.setApplicationName("EU-Digital Desktop");
    app.setOrganizationName("EU-Digital");
    app.setQuitOnLastWindowClosed(false); // Keeps running in tray

    eu_digital::DesktopController controller;
    controller.start();

    // Hook graceful shutdown
    QObject::connect(&app, &QApplication::aboutToQuit, [&controller]() {
        controller.stop();
    });

    return app.exec();
}
