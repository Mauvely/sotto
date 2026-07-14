#pragma once

#include <QObject>
#include <QString>

// Fire-and-forget desktop notifications over org.freedesktop.Notifications.
namespace Notifier {
void notify(const QString &summary, const QString &body = QString(), int timeoutMs = 4000);
}
