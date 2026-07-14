#include "hotkey/GlobalShortcutsPortal.h"

#include "portal/PortalRequest.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDebug>

namespace {

const QString kInterface = QStringLiteral("org.freedesktop.portal.GlobalShortcuts");
const QString kShortcutId = QStringLiteral("dictate");

struct PortalShortcut {
    QString id;
    QVariantMap props;
};
using PortalShortcutList = QList<PortalShortcut>;

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s)
{
    arg.beginStructure();
    arg << s.id << s.props;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s)
{
    arg.beginStructure();
    arg >> s.id >> s.props;
    arg.endStructure();
    return arg;
}

void registerTypes()
{
    static bool done = false;
    if (!done) {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<PortalShortcutList>();
        done = true;
    }
}

} // namespace

Q_DECLARE_METATYPE(PortalShortcut)
Q_DECLARE_METATYPE(PortalShortcutList)

GlobalShortcutsPortal::GlobalShortcutsPortal(QObject *parent)
    : QObject(parent)
{
    registerTypes();

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(Portal::kService, Portal::kPath, kInterface, QStringLiteral("Activated"), this,
                SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    bus.connect(Portal::kService, Portal::kPath, kInterface, QStringLiteral("Deactivated"), this,
                SLOT(onDeactivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
}

GlobalShortcutsPortal::~GlobalShortcutsPortal()
{
    release();
}

bool GlobalShortcutsPortal::available()
{
    return Portal::available();
}

void GlobalShortcutsPortal::bind(const QString &preferredTrigger)
{
    m_preferredTrigger = preferredTrigger;
    if (!m_sessionHandle.isEmpty()) {
        bindShortcuts();
        return;
    }

    QVariantMap options;
    options[QStringLiteral("session_handle_token")] = Portal::newToken();
    Portal::call(kInterface, QStringLiteral("CreateSession"), {options}, 0, this,
                 [this](uint code, const QVariantMap &results) {
                     if (code != 0) {
                         emit failed(tr("The GlobalShortcuts portal rejected the session (code %1).").arg(code));
                         return;
                     }
                     m_sessionHandle = results.value(QStringLiteral("session_handle")).toString();
                     bindShortcuts();
                 });
}

void GlobalShortcutsPortal::bindShortcuts()
{
    PortalShortcutList shortcuts;
    QVariantMap props;
    props[QStringLiteral("description")] = tr("Start/stop dictation");
    if (!m_preferredTrigger.isEmpty())
        props[QStringLiteral("preferred_trigger")] = m_preferredTrigger;
    shortcuts.append({kShortcutId, props});

    QVariantList args;
    args << QVariant::fromValue(QDBusObjectPath(m_sessionHandle));
    args << QVariant::fromValue(shortcuts);
    args << QString(); // parent_window
    args << QVariantMap();

    Portal::call(kInterface, QStringLiteral("BindShortcuts"), args, 3, this,
                 [this](uint code, const QVariantMap &results) {
                     if (code != 0) {
                         emit failed(tr("Binding the global shortcut failed (code %1).").arg(code));
                         return;
                     }
                     const auto list = qdbus_cast<PortalShortcutList>(
                         results.value(QStringLiteral("shortcuts")));
                     for (const PortalShortcut &s : list) {
                         if (s.id == kShortcutId) {
                             m_triggerDescription =
                                 s.props.value(QStringLiteral("trigger_description")).toString();
                         }
                     }
                     emit boundChanged(m_triggerDescription);
                 });
}

void GlobalShortcutsPortal::release()
{
    if (m_sessionHandle.isEmpty())
        return;
    Portal::closeSession(m_sessionHandle);
    m_sessionHandle.clear();
    m_triggerDescription.clear();
    emit boundChanged(QString());
}

void GlobalShortcutsPortal::onActivated(const QDBusObjectPath &session, const QString &shortcutId,
                                        qulonglong, const QVariantMap &)
{
    if (session.path() == m_sessionHandle && shortcutId == kShortcutId)
        emit activated();
}

void GlobalShortcutsPortal::onDeactivated(const QDBusObjectPath &session, const QString &shortcutId,
                                          qulonglong, const QVariantMap &)
{
    if (session.path() == m_sessionHandle && shortcutId == kShortcutId)
        emit deactivated();
}
