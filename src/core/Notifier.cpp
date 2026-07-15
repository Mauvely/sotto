#include "core/Notifier.h"

#ifdef Q_OS_WIN

#include <QPointer>
#include <QSystemTrayIcon>

namespace {
QPointer<QSystemTrayIcon> s_tray;
}

void Notifier::setFallbackTray(QSystemTrayIcon *tray)
{
    s_tray = tray;
}

void Notifier::notify(const QString &summary, const QString &body, int timeoutMs)
{
    if (s_tray)
        s_tray->showMessage(summary, body, QSystemTrayIcon::Information, timeoutMs);
}

#else // ------------------------------------------------------------- Linux

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantMap>

void Notifier::setFallbackTray(QSystemTrayIcon *)
{
}

void Notifier::notify(const QString &summary, const QString &body, int timeoutMs)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    msg << QStringLiteral("Sotto")            // app_name
        << quint32(0)                          // replaces_id
        << QStringLiteral("net.mauvely.sotto.app") // app_icon
        << summary
        << body
        << QStringList()                       // actions
        << QVariantMap()                       // hints
        << timeoutMs;
    QDBusConnection::sessionBus().asyncCall(msg);
}

#endif
