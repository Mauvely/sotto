#pragma once

#include <QObject>

class App;

// Session-bus interface io.github.timurinal.Sotto at /Sotto. Lets a second
// `sotto` invocation (or any compositor keybind running `sotto --toggle`,
// or plain `qdbus`/`gdbus`) drive the running instance.
class DBusService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.timurinal.Sotto")

public:
    static inline const QString kService = QStringLiteral("io.github.timurinal.sotto");
    static inline const QString kPath = QStringLiteral("/Sotto");
    static inline const QString kInterface = QStringLiteral("io.github.timurinal.Sotto");

    explicit DBusService(App *app, QObject *parent = nullptr);

    bool registerService();

    // Fire a method on an already-running instance; used by second launches.
    static void callRunningInstance(const QString &method);

public slots:
    Q_SCRIPTABLE void Toggle();
    Q_SCRIPTABLE void Stop();
    Q_SCRIPTABLE void ShowSettings();
    Q_SCRIPTABLE void ShowNotepad();
    Q_SCRIPTABLE void ShowChat();
    Q_SCRIPTABLE void Quit();

private:
    App *m_app;
};
