#include "core/DBusService.h"

#include "core/App.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>

DBusService::DBusService(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
{
    new LegacyDBusAdaptor(this); // remove next release; parented to this
}

bool DBusService::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kService))
        return false;

    // Remove next release. The old name is a lock too: an older Sotto that is
    // still running holds it, and then it is the primary instance. Two copies
    // would fight over the tray icon and the hotkey, so give the new name back
    // and let main() forward to the old one.
    if (!bus.registerService(kLegacyService)) {
        bus.unregisterService(kService);
        return false;
    }

    bus.registerObject(kPath, this,
                       QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportAdaptors);
    return true;
}

void DBusService::callRunningInstance(const QString &method)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
    // Remove next release: an older Sotto only answers to the old names.
    if (bus.interface() && !bus.interface()->isServiceRegistered(kService))
        msg = QDBusMessage::createMethodCall(kLegacyService, kPath, kLegacyInterface, method);
    bus.call(msg, QDBus::Block, 2000);
}

void DBusService::Toggle() { m_app->toggle(); }
void DBusService::Stop() { m_app->stopDictation(); }
void DBusService::ShowSettings() { m_app->showSettings(); }
void DBusService::ShowNotepad() { m_app->showNotepad(); }
void DBusService::Quit() { m_app->quit(); }

// Remove next release.
LegacyDBusAdaptor::LegacyDBusAdaptor(DBusService *service)
    : QDBusAbstractAdaptor(service)
    , m_service(service)
{
}

void LegacyDBusAdaptor::Toggle() { m_service->Toggle(); }
void LegacyDBusAdaptor::Stop() { m_service->Stop(); }
void LegacyDBusAdaptor::ShowSettings() { m_service->ShowSettings(); }
void LegacyDBusAdaptor::ShowNotepad() { m_service->ShowNotepad(); }
void LegacyDBusAdaptor::Quit() { m_service->Quit(); }
