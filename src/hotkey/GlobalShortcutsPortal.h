#pragma once

#include <QDBusObjectPath>
#include <QObject>
#include <QVariantMap>

// Global hotkey via the XDG Desktop Portal GlobalShortcuts interface.
// Works on KWin (Plasma ≥ 5.25) and Hyprland; the user can also manage the
// binding in Plasma System Settings → Shortcuts once bound.
//
// Emits activated() on key press and deactivated() on key release, which
// supports both toggle and push-to-talk modes.
class GlobalShortcutsPortal : public QObject
{
    Q_OBJECT
public:
    explicit GlobalShortcutsPortal(QObject *parent = nullptr);
    ~GlobalShortcutsPortal() override;

    // Creates the portal session and binds the shortcut. preferredTrigger
    // uses the XDG shortcuts spec syntax, e.g. "LOGO+ALT+d" or "CTRL+ALT+t".
    void bind(const QString &preferredTrigger);
    void release();
    bool bound() const { return !m_sessionHandle.isEmpty(); }

    // Human-readable description of the actual binding ("Meta+Alt+D").
    QString triggerDescription() const { return m_triggerDescription; }

    static bool available();

signals:
    void activated();
    void deactivated();
    void boundChanged(const QString &triggerDescription);
    void failed(const QString &message);

private slots:
    void onActivated(const QDBusObjectPath &session, const QString &shortcutId,
                     qulonglong timestamp, const QVariantMap &options);
    void onDeactivated(const QDBusObjectPath &session, const QString &shortcutId,
                       qulonglong timestamp, const QVariantMap &options);

private:
    void bindShortcuts();

    QString m_sessionHandle;
    QString m_preferredTrigger;
    QString m_triggerDescription;
};
