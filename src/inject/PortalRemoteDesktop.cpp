#include "inject/PortalRemoteDesktop.h"

#include "core/Settings.h"
#include "portal/PortalRequest.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QTimer>

namespace {
const QString kInterface = QStringLiteral("org.freedesktop.portal.RemoteDesktop");
constexpr int KEY_LEFTCTRL = 29; // linux/input-event-codes.h
constexpr int KEY_V = 47;
constexpr uint kDeviceKeyboard = 1;
constexpr uint kPersistUntilRevoked = 2;
} // namespace

PortalRemoteDesktop::PortalRemoteDesktop(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

PortalRemoteDesktop::~PortalRemoteDesktop()
{
    Portal::closeSession(m_sessionHandle);
}

bool PortalRemoteDesktop::available()
{
    return Portal::available();
}

void PortalRemoteDesktop::ensureSession(std::function<void(bool)> done)
{
    if (m_ready) {
        done(true);
        return;
    }

    QVariantMap createOpts;
    createOpts[QStringLiteral("session_handle_token")] = Portal::newToken();
    Portal::call(kInterface, QStringLiteral("CreateSession"), {createOpts}, 0, this,
                 [this, done](uint code, const QVariantMap &results) {
        if (code != 0) {
            done(false);
            return;
        }
        m_sessionHandle = results.value(QStringLiteral("session_handle")).toString();

        QVariantMap selectOpts;
        selectOpts[QStringLiteral("types")] = kDeviceKeyboard;
        selectOpts[QStringLiteral("persist_mode")] = kPersistUntilRevoked;
        const QString token = m_settings->remoteDesktopRestoreToken();
        if (!token.isEmpty())
            selectOpts[QStringLiteral("restore_token")] = token;

        QVariantList args{QVariant::fromValue(QDBusObjectPath(m_sessionHandle)), selectOpts};
        Portal::call(kInterface, QStringLiteral("SelectDevices"), args, 1, this,
                     [this, done](uint code, const QVariantMap &) {
            if (code != 0) {
                done(false);
                return;
            }
            QVariantList args{QVariant::fromValue(QDBusObjectPath(m_sessionHandle)),
                              QString(), // parent_window
                              QVariantMap()};
            Portal::call(kInterface, QStringLiteral("Start"), args, 2, this,
                         [this, done](uint code, const QVariantMap &results) {
                if (code != 0) {
                    done(false);
                    return;
                }
                const QString newToken =
                    results.value(QStringLiteral("restore_token")).toString();
                if (!newToken.isEmpty())
                    m_settings->setRemoteDesktopRestoreToken(newToken);
                m_ready = true;
                done(true);
            });
        });
    });
}

void PortalRemoteDesktop::notifyKeycode(int linuxKeycode, bool pressed)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        Portal::kService, Portal::kPath, kInterface, QStringLiteral("NotifyKeyboardKeycode"));
    msg << QVariant::fromValue(QDBusObjectPath(m_sessionHandle)) << QVariantMap()
        << linuxKeycode << uint(pressed ? 1 : 0);
    QDBusConnection::sessionBus().asyncCall(msg);
}

void PortalRemoteDesktop::sendPasteCombo(std::function<void(bool)> done)
{
    ensureSession([this, done](bool ok) {
        if (!ok) {
            done(false);
            return;
        }
        notifyKeycode(KEY_LEFTCTRL, true);
        QTimer::singleShot(25, this, [this, done] {
            notifyKeycode(KEY_V, true);
            QTimer::singleShot(25, this, [this, done] {
                notifyKeycode(KEY_V, false);
                notifyKeycode(KEY_LEFTCTRL, false);
                done(true);
            });
        });
    });
}
