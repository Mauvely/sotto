#include "core/apppaths.h"
#include "core/App.h"
#include "core/SingleInstance.h"
#include "core/Settings.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QQuickStyle>

#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
// NOMINMAX and WIN32_LEAN_AND_MEAN are set on the target in CMakeLists.txt.
#include <windows.h>
#endif

namespace {

void printHelp()
{
    std::puts("Sotto " SOTTO_VERSION " — fully local voice dictation\n"
              "\n"
              "Usage: MauvelySotto [option]\n"
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

#ifdef Q_OS_WIN
// ── Getting --help and --version onto a console at all ───────────────────────
//
// The target is WIN32_EXECUTABLE, i.e. the GUI subsystem, which is right for a
// tray app: a console subsystem binary flashes a black window on every launch.
// The cost is that the process starts with no stdout attached, so `sotto.exe
// --help` printed absolutely nothing and exited 0 — and CI's smoke test
// (`sotto --help`) would have "passed" while proving nothing at all.
//
// AttachConsole(ATTACH_PARENT_PROCESS) borrows the console of whoever launched
// it. It fails when there is no parent console — double-clicked from Explorer,
// or started by the autostart entry — and that is the correct outcome: there is
// nowhere to print, and we must not conjure a window.
//
// Reopening the CRT streams is the part that is easy to miss. Attaching a
// console does not rebind stdout, which is still the invalid handle the process
// started with, so printf keeps going nowhere until freopen points it at CONOUT$.
void attachParentConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        return;
    FILE *unused = nullptr;
    freopen_s(&unused, "CONOUT$", "w", stdout);
    freopen_s(&unused, "CONOUT$", "w", stderr);
    freopen_s(&unused, "CONIN$", "r", stdin);
}
#endif

int main(int argc, char *argv[])
{
    // Handle help/version before any GUI so they work headless.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help") || !std::strcmp(argv[i], "-h")) {
#ifdef Q_OS_WIN
            attachParentConsole();
#endif
            printHelp();
            return 0;
        }
        if (!std::strcmp(argv[i], "--version")) {
#ifdef Q_OS_WIN
            attachParentConsole();
#endif
            std::puts(SOTTO_VERSION);
            return 0;
        }
    }

    QCoreApplication::setOrganizationName(QStringLiteral(APP_ORGANISATION));
    QCoreApplication::setApplicationName(QStringLiteral(APP_DISPLAY_NAME));
    QCoreApplication::setApplicationVersion(QStringLiteral(SOTTO_VERSION));

    // Before anything constructs a QSettings or reaches for AppDataLocation.
    // Sotto used organisation "sotto", application "sotto" — its own naming,
    // not the suite's — so both halves changed. The data directory is the half
    // that matters here: it holds the downloaded speech models, and re-fetching
    // those is gigabytes rather than an inconvenience.
    apppaths::migrateOrganisation(QStringLiteral("sotto"), QStringLiteral("sotto"));

    QApplication app(argc, argv); // QApplication: needed for QSystemTrayIcon
    app.setQuitOnLastWindowClosed(false);
    QGuiApplication::setDesktopFileName(QStringLiteral("net.mauvely.sotto.app"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Mauvely brand faces: Baloo 2 (display), Inter (body/UI), JetBrains Mono
    // (labels/code). Bundled so the brand look doesn't depend on what's
    // installed system-wide; see resources/fonts/NOTICE.md for licensing.
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Baloo2-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Baloo2-ExtraBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/Inter-Variable.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/JetBrainsMono-Regular.ttf"));
    app.setFont(QFont(QStringLiteral("Inter")));

    const QStringList args = app.arguments().mid(1);

    // Single instance: if the lock is taken, forward the action and exit.
    Settings settings;
    App sotto(&settings);
    SingleInstance instance(&sotto);
    if (!instance.registerPrimary()) {
        QString method = QStringLiteral("ShowSettings");
        if (args.contains(QStringLiteral("--toggle")))
            method = QStringLiteral("Toggle");
        else if (args.contains(QStringLiteral("--stop")))
            method = QStringLiteral("Stop");
        else if (args.contains(QStringLiteral("--notepad")))
            method = QStringLiteral("ShowNotepad");
        else if (args.contains(QStringLiteral("--quit")))
            method = QStringLiteral("Quit");
        SingleInstance::forwardToRunning(method);
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
