#include "core/apppaths.h"
#include "core/App.h"
#include "core/SingleInstance.h"
#include "core/Settings.h"
#include "stt/ModelManager.h"

#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
// NOMINMAX and WIN32_LEAN_AND_MEAN are set on the target in CMakeLists.txt.
#include <windows.h>
#endif

namespace {

void printHelp()
{
    std::puts("Sotto " SOTTO_VERSION " — voice dictation\n"
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
              "  --help        this text");
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

    // Sotto keeps its configuration in an INI file on every platform, including
    // Windows, where QSettings would otherwise use the registry. Set as the
    // *default* format rather than passed to each constructor, because
    // apppaths::migrateOrganisation() below builds both its source and its
    // destination with the constructors that take no format — so this one call
    // is what makes the migration move the file the app actually reads. It used
    // to shuttle registry keys nobody had written while Settings read
    // sotto/sotto.conf regardless.
    QSettings::setDefaultFormat(QSettings::IniFormat);

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

    Settings settings;
    App sotto(&settings);

    // ── Download harness ─────────────────────────────────────────────────
    // The model downloader is the one thing in the app with no UI-free way to
    // exercise it, and "it does nothing and sits at 0%" is not a report anyone
    // can act on without watching the bytes. This drives ModelManager headless
    // and prints a line a second.
    //
    //   SOTTO_DOWNLOAD=tiny ./MauvelySotto            # 78 MB, the cheap probe
    //   SOTTO_DOWNLOAD=large-v3 SOTTO_DOWNLOAD_SECONDS=60 ./MauvelySotto
    //
    // Exits 0 when the model lands, 1 when it fails, 2 when the time cap ran
    // out first. It never touches the window, the tray or SingleInstance.
    const QString wanted = qEnvironmentVariable("SOTTO_DOWNLOAD");
    if (!wanted.isEmpty()) {
        // Deliberately *not* attachParentConsole(): borrowing the parent's
        // console re-points stderr at CONOUT$, which throws away a `2>` the
        // caller asked for. The screenshot harness below writes to stderr the
        // same way and for the same reason.
        auto *models = new ModelManager(&app);
        std::fprintf(stderr, "[dl] %s -> %s\n", qUtf8Printable(wanted),
                     qUtf8Printable(QDir::toNativeSeparators(ModelManager::modelsDir())));
        auto *tick = new QTimer(&app);
        tick->setInterval(1000);
        QObject::connect(tick, &QTimer::timeout, &app, [models] {
            std::fprintf(stderr, "[dl] %5.1f%%  %s\n", models->downloadProgress() * 100.0,
                         qUtf8Printable(models->downloadStatus()));
            std::fflush(stderr);
        });
        QObject::connect(models, &ModelManager::downloadFinished, &app,
                         [&app](const QString &id, bool ok, const QString &error) {
                             std::fprintf(stderr, "[dl] %s: %s%s%s\n", qUtf8Printable(id),
                                          ok ? "OK" : "FAILED", ok ? "" : " — ",
                                          ok ? "" : qUtf8Printable(error));
                             std::fflush(stderr);
                             app.exit(ok ? 0 : 1);
                         });
        const int cap = qEnvironmentVariableIntValue("SOTTO_DOWNLOAD_SECONDS");
        if (cap > 0)
            QTimer::singleShot(cap * 1000, &app, [&app, models] {
                std::fprintf(stderr, "[dl] time cap reached at %.1f%%\n",
                             models->downloadProgress() * 100.0);
                std::fflush(stderr);
                models->cancelDownload();
                app.exit(2);
            });
        tick->start();
        models->download(wanted);
        return app.exec();
    }

    // ── Screenshot harness ───────────────────────────────────────────────
    // Renders one window headless and exits, so the chrome can be looked at
    // without a display. The suite's other apps spell this CE_SHOT/SNAP_SHOT;
    // this is the QML equivalent.
    //
    //   QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    //   SOTTO_SHOT=/tmp/x.png SOTTO_VIEW=settings SOTTO_THEME=light \
    //     ./MauvelySotto
    //
    //   SOTTO_SHOT=<path>                     grab, then quit
    //   SOTTO_VIEW=settings|notepad|overlay   which window (default settings)
    //   SOTTO_THEME=dark|light|system         force a theme for this run only
    //   SOTTO_SIZE=<w>x<h>                    resize before grabbing — the
    //                                         settings page is several times
    //                                         taller than its window, and a
    //                                         640×760 grab shows two panels
    //   SOTTO_OPEN=combo                      open the language drop-down before
    //                                         grabbing. A popup is the one piece
    //                                         of chrome a shot of the settled
    //                                         window cannot show, and it is
    //                                         where the item text went missing.
    //   SOTTO_STATE=finalizing                with SOTTO_VIEW=overlay: render the
    //                                         HUD as it looks while whisper is
    //                                         still draining, rather than while
    //                                         listening
    //
    // It deliberately skips SingleInstance: a render is not a session, and
    // forwarding "ShowSettings" to a running copy and exiting would leave the
    // harness waiting for a file that is never written.
    const QString shot = qEnvironmentVariable("SOTTO_SHOT");
    if (!shot.isEmpty()) {
        const QString forcedTheme = qEnvironmentVariable("SOTTO_THEME");
        if (!forcedTheme.isEmpty())
            settings.setThemeOverride(forcedTheme);
        sotto.setHarnessOpen(qEnvironmentVariable("SOTTO_OPEN"));

        sotto.initialize();
        QQuickWindow *w = sotto.harnessWindow(qEnvironmentVariable("SOTTO_VIEW"));
        if (!w) {
            std::fputs("[shot] no window for SOTTO_VIEW\n", stderr);
            return 1;
        }
        const QString size = qEnvironmentVariable("SOTTO_SIZE");
        if (size.contains(u'x')) {
            const int sw = size.section(u'x', 0, 0).toInt();
            const int sh = size.section(u'x', 1, 1).toInt();
            if (sw > 0 && sh > 0)
                w->resize(sw, sh);
        }

        w->show();
        // Long enough for the first frame, the fonts and any Component.onCompleted
        // that sets a combo box's index. Nothing here is asynchronous the way
        // Snap's library scan is, so one delay covers every view.
        QTimer::singleShot(900, &app, [&sotto, w, shot] {
            if (w->grabWindow().save(shot))
                std::fprintf(stderr, "[shot] wrote %s\n", qUtf8Printable(shot));
            else
                std::fprintf(stderr, "[shot] failed to write %s\n", qUtf8Printable(shot));
            // App::quit(), not QCoreApplication::quit(): the windows have to go
            // before the QML engine does, or every binding on the App/Config
            // context properties re-evaluates against a null and the run ends in
            // a page of "TypeError: Cannot read property 'state' of null".
            sotto.quit();
        });
        return app.exec();
    }

    // Single instance: if the lock is taken, forward the action and exit.
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

    // Nothing was running, so there is nothing to stop. Starting a whole
    // background instance — tray icon, hotkey, the lot — in order to honour
    // `--quit` is the opposite of what was asked, and that is exactly what this
    // fell through to doing.
    if (args.contains(QStringLiteral("--quit")) || args.contains(QStringLiteral("--stop")))
        return 0;

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
