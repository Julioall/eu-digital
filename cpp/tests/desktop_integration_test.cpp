#include "shell/desktop_controller.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <QSettings>
#include <QElapsedTimer>
#include <iostream>

using namespace eu_digital;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("EU-Digital Test");
    app.setOrganizationName("EU-Digital");

    // Force consent to bypass modal dialog for test
    QSettings settings("EU-Digital", "DesktopRuntime");
    settings.setValue("consent_granted", true);

    DesktopController controller;
    
    QElapsedTimer timer;
    timer.start();

    // Start controller (simulating app start)
    controller.start();

    // The start should be fast (non-blocking because RuntimeHost goes to a thread)
    qint64 start_time = timer.elapsed();
    
    if (start_time > 500) { // Should be extremely fast, < 500ms
        std::cerr << "Start time too slow: " << start_time << "ms\n";
        return 1;
    }

    bool health_emitted = false;
    QObject::connect(&controller, &DesktopController::healthUpdated, [&](const QString& json) {
        health_emitted = true;
    });

    // Run event loop to process timers
    QTimer::singleShot(2000, [&]() {
        controller.stop();
        app.quit();
    });

    app.exec();

    if (!health_emitted) {
        std::cerr << "Health status was not emitted by the controller.\n";
        return 1;
    }

    std::cout << "Desktop integration test passed.\n";
    return 0;
}
