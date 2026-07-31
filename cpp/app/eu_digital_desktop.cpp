#include "shell/desktop_controller.hpp"
#include <QApplication>

int main(int argc, char** argv) {
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
