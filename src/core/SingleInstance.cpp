#include "core/SingleInstance.h"

#include "core/App.h"

#ifdef Q_OS_WIN

#include <QLocalServer>
#include <QLocalSocket>

namespace {
const QString kServerName = QStringLiteral("net.mauvely.sotto");
}

SingleInstance::SingleInstance(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
{
}

bool SingleInstance::registerPrimary()
{
    m_server = new QLocalServer(this);
    // Windows named pipes disappear with their process, so a failed listen
    // means a live instance (no stale-socket cleanup dance needed).
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

bool SingleInstance::registerPrimary()
{
    return m_dbus->registerService();
}

void SingleInstance::forwardToRunning(const QString &method)
{
    DBusService::callRunningInstance(method);
}

#endif
