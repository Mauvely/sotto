#pragma once

#include <QDBusAbstractAdaptor>
#include <QObject>

class App;

// Session-bus interface com.mauvely.Sotto at /Sotto, on the bus name
// com.mauvely.sotto. Lets a second `sotto` invocation (or any compositor
// keybind running `sotto --toggle`, or plain `qdbus`/`gdbus`) drive the running
// instance.
//
// These names are written out rather than derived from APP_ID: people script
// against them, so they change when the documented API changes and not when
// the app's id does.
class DBusService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.mauvely.Sotto")

public:
    static inline const QString kService = QStringLiteral("com.mauvely.sotto");
    static inline const QString kPath = QStringLiteral("/Sotto");
    static inline const QString kInterface = QStringLiteral("com.mauvely.Sotto");

    // ── Remove next release ──────────────────────────────────────────────
    // Until 2026-10-14 the names were net.mauvely.sotto and net.mauvely.Sotto.
    // For one release the same object is also served under them, so a script
    // written against the old ones keeps working. See LegacyDBusAdaptor below
    // and the places that mention kLegacyService in DBusService.cpp, and the
    // --own-name line in packaging/linux/flatpak/app.yml.in.
    static inline const QString kLegacyService = QStringLiteral("net.mauvely.sotto");
    static inline const QString kLegacyInterface = QStringLiteral("net.mauvely.Sotto");

    explicit DBusService(App *app, QObject *parent = nullptr);

    bool registerService();

    // Fire a method on an already-running instance; used by second launches.
    static void callRunningInstance(const QString &method);

public slots:
    Q_SCRIPTABLE void Toggle();
    Q_SCRIPTABLE void Stop();
    Q_SCRIPTABLE void ShowSettings();
    Q_SCRIPTABLE void ShowNotepad();
    Q_SCRIPTABLE void Quit();

private:
    App *m_app;
};

// Remove next release, with the kLegacy* names above. Exports the same five
// methods as net.mauvely.Sotto, on the same object, by forwarding to the
// DBusService it is parented to.
class LegacyDBusAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.mauvely.Sotto")

public:
    explicit LegacyDBusAdaptor(DBusService *service);

public slots:
    void Toggle();
    void Stop();
    void ShowSettings();
    void ShowNotepad();
    void Quit();

private:
    DBusService *m_service;
};
