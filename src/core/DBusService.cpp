#include "core/DBusService.h"

#include "core/App.h"

#include <QDBusConnection>
#include <QDBusMessage>

DBusService::DBusService(App *app, QObject *parent)
    : QObject(parent)
    , m_app(app)
{
}

bool DBusService::registerService()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(kService))
        return false;
    bus.registerObject(kPath, this, QDBusConnection::ExportScriptableSlots);
    return true;
}

void DBusService::callRunningInstance(const QString &method)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kInterface, method);
    QDBusConnection::sessionBus().call(msg, QDBus::Block, 2000);
}

void DBusService::Toggle() { m_app->toggle(); }
void DBusService::Stop() { m_app->stopDictation(); }
void DBusService::ShowSettings() { m_app->showSettings(); }
void DBusService::ShowNotepad() { m_app->showNotepad(); }
void DBusService::ShowChat() { m_app->showChat(); }
void DBusService::Quit() { m_app->quit(); }
