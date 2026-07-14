#include "portal/PortalRequest.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>

namespace Portal {

namespace {

class ResponseListener : public QObject
{
    Q_OBJECT
public:
    ResponseListener(QString path, ResponseHandler handler, QObject *context)
        : QObject(context)
        , m_path(std::move(path))
        , m_handler(std::move(handler))
    {
        subscribe(m_path);
    }

    void subscribe(const QString &path)
    {
        QDBusConnection::sessionBus().connect(kService, path,
                                              QStringLiteral("org.freedesktop.portal.Request"),
                                              QStringLiteral("Response"), this,
                                              SLOT(onResponse(uint,QVariantMap)));
    }

    void fail()
    {
        onResponse(2, QVariantMap());
    }

public slots:
    void onResponse(uint code, const QVariantMap &results)
    {
        if (m_done)
            return;
        m_done = true;
        if (m_handler)
            m_handler(code, results);
        deleteLater();
    }

private:
    QString m_path;
    ResponseHandler m_handler;
    bool m_done = false;
};

} // namespace

QString newToken()
{
    static quint64 counter = 0;
    return QStringLiteral("sotto_%1").arg(++counter);
}

bool available()
{
    auto *iface = QDBusConnection::sessionBus().interface();
    return iface && iface->isServiceRegistered(kService);
}

void call(const QString &interface, const QString &method, QVariantList args,
          int optionsArgIndex, QObject *context, ResponseHandler handler)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    const QString token = newToken();
    QVariantMap options = args.at(optionsArgIndex).toMap();
    options[QStringLiteral("handle_token")] = token;
    args[optionsArgIndex] = options;

    // Expected request object path per the portal spec.
    QString sender = bus.baseService().mid(1);
    sender.replace(u'.', u'_');
    const QString expectedPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    auto *listener = new ResponseListener(expectedPath, std::move(handler), context);

    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, interface, method);
    msg.setArguments(args);

    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg), listener);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, listener,
                     [listener](QDBusPendingCallWatcher *w) {
                         QDBusPendingReply<QDBusObjectPath> reply = *w;
                         w->deleteLater();
                         if (reply.isError()) {
                             qWarning() << "portal call failed:" << reply.error().message();
                             listener->fail();
                             return;
                         }
                         // Older portals may return a different request path;
                         // listen there as well.
                         listener->subscribe(reply.value().path());
                     });
}

void registerHostApp(const QString &appId)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, kPath, QStringLiteral("org.freedesktop.host.portal.Registry"),
        QStringLiteral("Register"));
    msg << appId << QVariantMap();

    QDBusConnection bus = QDBusConnection::sessionBus();
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(msg));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [watcher](QDBusPendingCallWatcher *w) {
                         QDBusPendingReply<> reply = *w;
                         w->deleteLater();
                         if (reply.isError()) {
                             // Portals predating 1.20 don't have this interface at
                             // all; that's fine, GlobalShortcuts et al. simply
                             // won't enforce an app id there.
                             qDebug() << "host portal Register() failed (harmless on"
                                         " portal < 1.20):" << reply.error().message();
                         }
                     });
}

void closeSession(const QString &sessionHandle)
{
    if (sessionHandle.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        kService, sessionHandle, QStringLiteral("org.freedesktop.portal.Session"),
        QStringLiteral("Close"));
    QDBusConnection::sessionBus().asyncCall(msg);
}

} // namespace Portal

#include "PortalRequest.moc"
