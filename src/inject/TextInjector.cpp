#include "inject/TextInjector.h"

#include "core/Settings.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>

#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#include <vector>
#else
#include "inject/PortalRemoteDesktop.h"
#include <QProcess>
#include <QTemporaryFile>
#endif

namespace {

void setClipboard(const QString &text)
{
#ifndef Q_OS_WIN
    if (TextInjector::haveWlClipboard()) {
        auto *p = new QProcess;
        p->start(QStringLiteral("wl-copy"),
                 {QStringLiteral("--type"), QStringLiteral("text/plain;charset=utf-8")});
        if (p->waitForStarted(1000)) {
            p->write(text.toUtf8());
            p->closeWriteChannel();
            // wl-copy forks a child to serve the selection and exits.
            QObject::connect(p, &QProcess::finished, p, &QObject::deleteLater);
            QTimer::singleShot(3000, p, &QObject::deleteLater);
            return;
        }
        delete p;
    }
#endif
    QGuiApplication::clipboard()->setText(text);
}

QString readClipboard()
{
#ifndef Q_OS_WIN
    if (TextInjector::haveWlClipboard()) {
        QProcess p;
        p.start(QStringLiteral("wl-paste"), {QStringLiteral("--no-newline")});
        if (!p.waitForFinished(500) || p.exitCode() != 0)
            return QString();
        const QByteArray data = p.readAllStandardOutput();
        if (data.size() > 1'000'000) // don't try to restore huge payloads
            return QString();
        return QString::fromUtf8(data);
    }
#endif
    return QGuiApplication::clipboard()->text();
}

// ---------------------------------------------------------------------------

class ClipboardOnlyInjector : public TextInjector
{
public:
    using TextInjector::TextInjector;
    void inject(const QString &text) override
    {
        setClipboard(text);
        emit finished(true, tr("Copied to clipboard"));
    }
};

#ifdef Q_OS_WIN

// Ctrl+V through SendInput.
void sendPasteKeystroke()
{
    INPUT in[4] = {};
    for (auto &i : in)
        i.type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_CONTROL;
    in[1].ki.wVk = 'V';
    in[2].ki.wVk = 'V';
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    in[3].ki.wVk = VK_CONTROL;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, in, sizeof(INPUT));
}

// Types the text as KEYEVENTF_UNICODE events — layout-independent, and
// surrogate pairs pass through as two consecutive units, which is exactly
// what the API expects.
class WinTypeInjector : public TextInjector
{
public:
    using TextInjector::TextInjector;
    void inject(const QString &text) override
    {
        std::vector<INPUT> in;
        in.reserve(size_t(text.size()) * 2);
        for (const QChar c : text) {
            if (c == u'\r')
                continue;
            INPUT down = {};
            down.type = INPUT_KEYBOARD;
            if (c == u'\n') {
                down.ki.wVk = VK_RETURN;
            } else {
                down.ki.dwFlags = KEYEVENTF_UNICODE;
                down.ki.wScan = c.unicode();
            }
            INPUT up = down;
            up.ki.dwFlags |= KEYEVENTF_KEYUP;
            in.push_back(down);
            in.push_back(up);
        }
        if (in.empty()) {
            emit finished(true, QString());
            return;
        }
        const UINT sent = SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
        if (sent == in.size())
            emit finished(true, QString());
        else
            emit finished(false, tr("Typing the text failed (input was blocked)."));
    }
};

#else // ------------------------------------------------------------- Linux

class YdotoolTypeInjector : public TextInjector
{
public:
    using TextInjector::TextInjector;
    void inject(const QString &text) override
    {
        auto *tmp = new QTemporaryFile(this);
        if (!tmp->open()) {
            emit finished(false, tr("Could not create a temporary file."));
            return;
        }
        tmp->write(text.toUtf8());
        tmp->flush();

        auto *p = new QProcess(this);
        connect(p, &QProcess::finished, this, [this, p, tmp](int code, QProcess::ExitStatus) {
            tmp->deleteLater();
            p->deleteLater();
            if (code == 0)
                emit finished(true, QString());
            else
                emit finished(false, tr("ydotool failed (exit code %1).").arg(code));
        });
        p->start(QStringLiteral("ydotool"),
                 {QStringLiteral("type"), QStringLiteral("--file"), tmp->fileName()});
    }
};

#endif

class ClipboardPasteInjector : public TextInjector
{
public:
    ClipboardPasteInjector(Settings *settings, PortalRemoteDesktop *portalRd, QObject *parent)
        : TextInjector(parent)
        , m_settings(settings)
        , m_portalRd(portalRd)
    {
    }

    void inject(const QString &text) override
    {
        const QString saved = m_settings->restoreClipboard() ? readClipboard() : QString();
        setClipboard(text);

        // Give the compositor a moment to see the new selection, then paste.
        QTimer::singleShot(150, this, [this, saved] {
            sendPaste([this, saved](bool ok) {
                if (ok && !saved.isEmpty()) {
                    QTimer::singleShot(1000, this, [saved] { setClipboard(saved); });
                }
                if (ok)
                    emit finished(true, QString());
                else
                    emit finished(false,
                                  tr("Could not synthesize Ctrl+V — the text is on the clipboard; "
                                     "paste it manually. Set up ydotool or allow the RemoteDesktop "
                                     "portal for automatic pasting."));
            });
        });
    }

private:
    void sendPaste(std::function<void(bool)> done)
    {
#ifdef Q_OS_WIN
        sendPasteKeystroke();
        done(true);
#else
        if (ydotoolReady()) {
            auto *p = new QProcess(this);
            connect(p, &QProcess::finished, this,
                    [p, done](int code, QProcess::ExitStatus) {
                        p->deleteLater();
                        done(code == 0);
                    });
            // 29 = KEY_LEFTCTRL, 47 = KEY_V
            p->start(QStringLiteral("ydotool"),
                     {QStringLiteral("key"), QStringLiteral("29:1"), QStringLiteral("47:1"),
                      QStringLiteral("47:0"), QStringLiteral("29:0")});
            return;
        }
        if (m_portalRd) {
            m_portalRd->sendPasteCombo(std::move(done));
            return;
        }
        done(false);
#endif
    }

    Settings *m_settings;
    PortalRemoteDesktop *m_portalRd;
};

} // namespace

// ---------------------------------------------------------------------------

bool TextInjector::ydotoolReady()
{
#ifdef Q_OS_WIN
    return false;
#else
    if (QStandardPaths::findExecutable(QStringLiteral("ydotool")).isEmpty())
        return false;
    const QString socket = qEnvironmentVariable(
        "YDOTOOL_SOCKET",
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
            + QStringLiteral("/.ydotool_socket"));
    return QFile::exists(socket);
#endif
}

bool TextInjector::haveWlClipboard()
{
#ifdef Q_OS_WIN
    return false;
#else
    return !QStandardPaths::findExecutable(QStringLiteral("wl-copy")).isEmpty();
#endif
}

QString TextInjector::diagnostics()
{
#ifdef Q_OS_WIN
    return tr("Text is inserted with native Windows input (SendInput).");
#else
    QStringList found, missing;
    (haveWlClipboard() ? found : missing) << QStringLiteral("wl-clipboard");
    (ydotoolReady() ? found : missing) << QStringLiteral("ydotool");
    (PortalRemoteDesktop::available() ? found : missing) << QStringLiteral("RemoteDesktop portal");
    QString s = tr("Detected: %1").arg(found.isEmpty() ? tr("nothing") : found.join(QStringLiteral(", ")));
    if (!missing.isEmpty())
        s += tr(" — missing: %1").arg(missing.join(QStringLiteral(", ")));
    return s;
#endif
}

TextInjector *TextInjector::create(Settings *settings, PortalRemoteDesktop *portalRd,
                                   QObject *parent)
{
    const QString mode = settings->injectionMode();

#ifdef Q_OS_WIN
    Q_UNUSED(portalRd)
    // "ydotool-type" is the stored value of the "Type it" choice; the
    // config key is kept for portability of sotto.conf across machines.
    if (mode == QStringLiteral("ydotool-type"))
        return new WinTypeInjector(parent);
    if (mode == QStringLiteral("clipboard-only"))
        return new ClipboardOnlyInjector(parent);
    return new ClipboardPasteInjector(settings, nullptr, parent);
#else
    if (mode == QStringLiteral("ydotool-type") && ydotoolReady())
        return new YdotoolTypeInjector(parent);

    if (mode == QStringLiteral("clipboard-only"))
        return new ClipboardOnlyInjector(parent);

    // "clipboard-paste" or "auto": paste needs a keystroke backend.
    if (mode != QStringLiteral("clipboard-only")
        && (ydotoolReady() || PortalRemoteDesktop::available())) {
        return new ClipboardPasteInjector(settings, portalRd, parent);
    }

    return new ClipboardOnlyInjector(parent);
#endif
}
