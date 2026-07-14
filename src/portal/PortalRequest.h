#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

// Helpers for talking to xdg-desktop-portal interfaces that follow the
// Request pattern (CreateSession, BindShortcuts, SelectDevices, Start, ...):
// a handle_token is generated, the matching Request.Response signal is
// subscribed *before* the method call, and the handler runs exactly once.
namespace Portal {

inline const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
inline const QString kPath = QStringLiteral("/org/freedesktop/portal/desktop");

using ResponseHandler = std::function<void(uint code, const QVariantMap &results)>;

// `args` must already contain the a{sv} options map at `optionsArgIndex`;
// the generated handle_token is inserted into it. `context` scopes the
// callback lifetime. code: 0 = success, 1 = user cancelled, 2 = error.
void call(const QString &interface, const QString &method, QVariantList args,
          int optionsArgIndex, QObject *context, ResponseHandler handler);

QString newToken();

// True if the portal service is reachable on the session bus.
bool available();

// Close a portal session object (org.freedesktop.portal.Session.Close).
void closeSession(const QString &sessionHandle);

// xdg-desktop-portal >= 1.20 requires non-sandboxed ("host") apps to
// self-report their app id via org.freedesktop.host.portal.Registry before
// using identity-sensitive portals; >= 1.21 hard-rejects GlobalShortcuts
// sessions with an empty app id otherwise ("An app id is required"). Call
// this once at startup, before any other portal call.
void registerHostApp(const QString &appId);

} // namespace Portal
