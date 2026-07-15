#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QTimer>

// Global hotkey via the Win32 RegisterHotKey API. Same surface as
// GlobalShortcutsPortal so App can use either behind the HotkeyBackend
// alias, including the same trigger spec syntax ("LOGO+ALT+d" — LOGO is
// the Windows key here).
//
// WM_HOTKEY only reports presses, so key release (hold-to-talk) is
// detected by polling GetAsyncKeyState on the bound key after each
// activation.
class WinHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit WinHotkey(QObject *parent = nullptr);
    ~WinHotkey() override;

    void bind(const QString &preferredTrigger);
    void release();
    bool bound() const { return m_bound; }

    // Human-readable description of the actual binding ("Win+Alt+D").
    QString triggerDescription() const { return m_triggerDescription; }

    static bool available() { return true; }

    bool nativeEventFilter(const QByteArray &eventType, void *message,
                           qintptr *result) override;

signals:
    void activated();
    void deactivated();
    void boundChanged(const QString &triggerDescription);
    void failed(const QString &message);

private:
    void pollRelease();

    quint32 m_mods = 0;
    quint32 m_vk = 0;
    bool m_bound = false;
    bool m_down = false;
    QTimer m_releasePoll;
    QString m_triggerDescription;
};
