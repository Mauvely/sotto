#pragma once

#include <QObject>
#include <QString>

class Settings;
class PortalRemoteDesktop;

// Delivers the finished transcript into the focused application.
// Wayland has no universal "type text" API, so this is a strategy with
// auto-detection:
//
//   clipboard-paste : set the clipboard (wl-copy) and synthesize Ctrl+V via
//                     ydotool if present, else the RemoteDesktop portal;
//                     original clipboard restored afterwards. Default.
//   ydotool-type    : type the text directly through uinput (ydotool).
//   clipboard-only  : just copy + notify; nothing is injected.
class TextInjector : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // Picks the backend based on settings->injectionMode() and what is
    // available on the system. Never returns null (falls back to
    // clipboard-only). The caller owns the returned object.
    static TextInjector *create(Settings *settings, PortalRemoteDesktop *portalRd,
                                QObject *parent);

    // One-line description of what auto-detection found, for the settings UI.
    static QString diagnostics();

    static bool ydotoolReady();
    static bool haveWlClipboard();

    virtual void inject(const QString &text) = 0;

signals:
    void finished(bool ok, const QString &message);
};
