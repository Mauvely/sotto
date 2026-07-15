#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;

// Fire-and-forget desktop notifications: org.freedesktop.Notifications on
// Linux, tray balloons (registered by TrayIcon) elsewhere.
namespace Notifier {
void notify(const QString &summary, const QString &body = QString(), int timeoutMs = 4000);

// Where the platform has no notification bus, messages go through this
// tray icon instead. No-op on Linux.
void setFallbackTray(QSystemTrayIcon *tray);
}
