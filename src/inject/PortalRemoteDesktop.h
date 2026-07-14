#pragma once

#include <QObject>

#include <functional>

class Settings;

// Keystroke synthesis through the XDG RemoteDesktop portal — the
// compositor-sanctioned way to send key events on Wayland (KWin, GNOME).
// Used to press Ctrl+V for clipboard-paste injection when ydotool is not
// set up. The permission dialog appears once; the grant is persisted via
// the portal restore token.
class PortalRemoteDesktop : public QObject
{
    Q_OBJECT
public:
    explicit PortalRemoteDesktop(Settings *settings, QObject *parent = nullptr);
    ~PortalRemoteDesktop() override;

    // Ensures the session exists (may show a system permission dialog the
    // first time), then taps Ctrl+V.
    void sendPasteCombo(std::function<void(bool ok)> done);

    static bool available();

private:
    void ensureSession(std::function<void(bool ok)> done);
    void notifyKeycode(int linuxKeycode, bool pressed);

    Settings *m_settings;
    QString m_sessionHandle;
    bool m_ready = false;
};
