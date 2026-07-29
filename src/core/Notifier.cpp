#include "core/Notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QVariantMap>

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
