#pragma once

#include <QObject>
#include <QString>

class App;
class DBusService;
class QLocalServer;

// Guarantees one Sotto per session and lets a second invocation forward
// its command line to the running instance.
//
// Linux: wraps DBusService — the session-bus name doubles as the lock and
// the interface stays scriptable via qdbus/gdbus.
// Windows: a QLocalServer named pipe carrying one command word
// ("Toggle", "ShowSettings", …) per connection.
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(App *app, QObject *parent = nullptr);

    // True if this process is now the primary instance.
    bool registerPrimary();

    // Fire a method on the already-running instance; used by second launches.
    static void forwardToRunning(const QString &method);

private:
    App *m_app;
#ifdef Q_OS_WIN
    QLocalServer *m_server = nullptr;
#else
    DBusService *m_dbus = nullptr;
#endif
};
