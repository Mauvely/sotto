#include "core/App.h"
#include "core/DBusService.h"
#include "core/Settings.h"

#include <QApplication>
#include <QQuickStyle>

#include <cstdio>
#include <cstring>

namespace {

void printHelp()
{
    std::puts("Sotto " SOTTO_VERSION " — fully local voice dictation for Linux\n"
              "\n"
              "Usage: sotto [option]\n"
              "\n"
              "  (no option)   start in the background (tray + global shortcut)\n"
              "  --toggle      start/stop dictation in the running instance\n"
              "  --stop        stop dictation in the running instance\n"
              "  --settings    open the settings window\n"
              "  --notepad     open the notepad window\n"
              "  --quit        quit the running instance\n"
              "  --version     print the version and exit\n"
              "  --help        this text\n"
              "\n"
              "All speech recognition runs on this machine. Nothing is sent anywhere.");
}

} // namespace

int main(int argc, char *argv[])
{
    // Handle help/version before any GUI so they work headless.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
            printHelp();
            return 0;
        }
        if (!std::strcmp(argv[i], "--version")) {
            std::puts(SOTTO_VERSION);
            return 0;
        }
    }

    QCoreApplication::setOrganizationName(QStringLiteral("sotto"));
    QCoreApplication::setApplicationName(QStringLiteral("sotto"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SOTTO_VERSION));

    QApplication app(argc, argv); // QApplication: needed for QSystemTrayIcon
    app.setQuitOnLastWindowClosed(false);
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.timurinal.sotto"));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    const QStringList args = app.arguments().mid(1);

    // Single instance: if the service is taken, forward the action and exit.
    Settings settings;
    App sotto(&settings);
    DBusService dbus(&sotto);
    if (!dbus.registerService()) {
        QString method = QStringLiteral("ShowSettings");
        if (args.contains(QStringLiteral("--toggle")))
            method = QStringLiteral("Toggle");
        else if (args.contains(QStringLiteral("--stop")))
            method = QStringLiteral("Stop");
        else if (args.contains(QStringLiteral("--notepad")))
            method = QStringLiteral("ShowNotepad");
        else if (args.contains(QStringLiteral("--quit")))
            method = QStringLiteral("Quit");
        DBusService::callRunningInstance(method);
        return 0;
    }

    sotto.initialize();

    if (args.contains(QStringLiteral("--settings")))
        sotto.showSettings();
    else if (args.contains(QStringLiteral("--notepad")))
        sotto.showNotepad();
    else if (args.contains(QStringLiteral("--toggle")))
        QMetaObject::invokeMethod(&sotto, &App::toggle, Qt::QueuedConnection);
    else if (settings.firstRun()) {
        settings.setFirstRunDone();
        sotto.showSettings();
    }

    return app.exec();
}
