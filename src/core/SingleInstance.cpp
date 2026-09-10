#include "core/SingleInstance.h"

#include "core/App.h"

#ifdef Q_OS_WIN

#include <QLocalServer>
#include <QLocalSocket>

#include <windows.h>

namespace {
const QString kServerName = QStringLiteral("net.mauvely.sotto");
// Per user session, not machine-wide: two people logged into the same box each
// get their own Sotto, and "Local\" needs no privilege that "Global\" does.
const wchar_t *kLockName = L"Local\\net.mauvely.sotto.instance";
}

SingleInstance::SingleInstance(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
{
}

SingleInstance::~SingleInstance()
{
    if (m_lock)
        CloseHandle(static_cast<HANDLE>(m_lock));
}

bool SingleInstance::registerPrimary()
{
    // ── The pipe cannot be the lock ──────────────────────────────────────
    // This used to be `return m_server->listen(kServerName)` alone, on the
    // reasoning that a Windows named pipe dies with its process so a failed
    // listen means a live instance. The premise is right and the conclusion is
    // not: Qt creates the pipe with PIPE_UNLIMITED_INSTANCES and *without*
    // FILE_FLAG_FIRST_PIPE_INSTANCE, so a second server on the same name gets
    // another instance of it and listen() succeeds. Every copy then believed it
    // was the only one. Measured on 2026-09-10: two `MauvelySotto --settings`
    // both stayed running, with two tray icons, and the second's RegisterHotKey
    // losing to the first — and `--quit`/`--toggle` from a shortcut started a
    // third background copy instead of talking to the running one.
    //
    // A named mutex is the lock. The pipe stays as the transport, and only the
    // process holding the mutex ever listens on it.
    m_lock = CreateMutexW(nullptr, FALSE, kLockName);
    if (!m_lock || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (m_lock) {
            CloseHandle(static_cast<HANDLE>(m_lock));
            m_lock = nullptr;
        }
        return false;
    }

    m_server = new QLocalServer(this);
    if (!m_server->listen(kServerName))
        return false;

    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *sock = m_server->nextPendingConnection()) {
            connect(sock, &QLocalSocket::readyRead, this, [this, sock] {
                const QString method = QString::fromUtf8(sock->readLine()).trimmed();
                if (method == QStringLiteral("Toggle"))
                    m_app->toggle();
                else if (method == QStringLiteral("Stop"))
                    m_app->stopDictation();
                else if (method == QStringLiteral("ShowSettings"))
                    m_app->showSettings();
                else if (method == QStringLiteral("ShowNotepad"))
                    m_app->showNotepad();
                else if (method == QStringLiteral("Quit"))
                    m_app->quit();
                sock->disconnectFromServer();
            });
            connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
        }
    });
    return true;
}

void SingleInstance::forwardToRunning(const QString &method)
{
    QLocalSocket sock;
    sock.connectToServer(kServerName);
    if (!sock.waitForConnected(2000))
        return;
    sock.write(method.toUtf8() + '\n');
    sock.waitForBytesWritten(2000);
    sock.disconnectFromServer();
    if (sock.state() != QLocalSocket::UnconnectedState)
        sock.waitForDisconnected(1000);
}

#else // ------------------------------------------------------------- Linux

#include "core/DBusService.h"

SingleInstance::SingleInstance(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_dbus(new DBusService(app, this))
{
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::registerPrimary()
{
    return m_dbus->registerService();
}

void SingleInstance::forwardToRunning(const QString &method)
{
    DBusService::callRunningInstance(method);
}

#endif
