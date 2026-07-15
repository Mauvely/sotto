#include "hotkey/WinHotkey.h"

#include <QCoreApplication>
#include <QStringList>

#include <windows.h>

namespace {

constexpr int kHotkeyId = 0x5054; // arbitrary, unique within this process

// Parses the XDG-style trigger spec shared with the portal backend.
// Returns false if a token isn't understood.
bool parseTrigger(const QString &spec, quint32 &mods, quint32 &vk, QString &description)
{
    mods = 0;
    vk = 0;
    QStringList pretty;
    const QStringList tokens = spec.split(u'+', Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token = token.trimmed();
        const QString t = token.toUpper();
        if (t == QStringLiteral("CTRL") || t == QStringLiteral("CONTROL")) {
            mods |= MOD_CONTROL;
            pretty << QStringLiteral("Ctrl");
        } else if (t == QStringLiteral("ALT")) {
            mods |= MOD_ALT;
            pretty << QStringLiteral("Alt");
        } else if (t == QStringLiteral("SHIFT")) {
            mods |= MOD_SHIFT;
            pretty << QStringLiteral("Shift");
        } else if (t == QStringLiteral("LOGO") || t == QStringLiteral("META")
                   || t == QStringLiteral("SUPER") || t == QStringLiteral("WIN")) {
            mods |= MOD_WIN;
            pretty << QStringLiteral("Win");
        } else if (t.size() == 1
                   && ((t.at(0) >= u'A' && t.at(0) <= u'Z')
                       || (t.at(0) >= u'0' && t.at(0) <= u'9'))) {
            vk = t.at(0).unicode(); // VK codes for A–Z / 0–9 match ASCII
            pretty << t;
        } else if (t == QStringLiteral("SPACE")) {
            vk = VK_SPACE;
            pretty << QStringLiteral("Space");
        } else if (t.size() >= 2 && t.at(0) == u'F') {
            bool ok = false;
            const int n = t.mid(1).toInt(&ok);
            if (!ok || n < 1 || n > 24)
                return false;
            vk = VK_F1 + n - 1;
            pretty << t;
        } else {
            return false;
        }
    }
    description = pretty.join(u'+');
    return vk != 0;
}

} // namespace

WinHotkey::WinHotkey(QObject *parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
    m_releasePoll.setInterval(50);
    connect(&m_releasePoll, &QTimer::timeout, this, &WinHotkey::pollRelease);
}

WinHotkey::~WinHotkey()
{
    release();
    if (auto *app = QCoreApplication::instance())
        app->removeNativeEventFilter(this);
}

void WinHotkey::bind(const QString &preferredTrigger)
{
    release();

    quint32 mods = 0, vk = 0;
    QString description;
    if (!parseTrigger(preferredTrigger, mods, vk, description)) {
        emit failed(tr("Could not understand the shortcut \"%1\" — use e.g. LOGO+ALT+d "
                       "(modifiers CTRL/ALT/SHIFT/LOGO plus a letter, digit, F-key or SPACE).")
                        .arg(preferredTrigger));
        return;
    }

    // MOD_NOREPEAT: don't refire while the key autorepeats.
    if (!RegisterHotKey(nullptr, kHotkeyId, mods | MOD_NOREPEAT, vk)) {
        emit failed(tr("Windows refused the shortcut %1 — probably taken by another "
                       "application. Pick different keys in Settings.")
                        .arg(description));
        return;
    }

    m_mods = mods;
    m_vk = vk;
    m_bound = true;
    m_triggerDescription = description;
    emit boundChanged(m_triggerDescription);
}

void WinHotkey::release()
{
    if (!m_bound)
        return;
    UnregisterHotKey(nullptr, kHotkeyId);
    m_bound = false;
    m_down = false;
    m_releasePoll.stop();
    m_triggerDescription.clear();
    emit boundChanged(m_triggerDescription);
}

bool WinHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *)
{
    if (eventType != "windows_generic_MSG")
        return false;
    const MSG *msg = static_cast<MSG *>(message);
    if (msg->message != WM_HOTKEY || int(msg->wParam) != kHotkeyId)
        return false;
    if (!m_down) {
        m_down = true;
        emit activated();
        m_releasePoll.start();
    }
    return true;
}

void WinHotkey::pollRelease()
{
    if (GetAsyncKeyState(int(m_vk)) & 0x8000)
        return; // still held
    m_down = false;
    m_releasePoll.stop();
    emit deactivated();
}
