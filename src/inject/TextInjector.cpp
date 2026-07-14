#include "inject/TextInjector.h"

#include "core/Settings.h"
#include "inject/PortalRemoteDesktop.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

namespace {

void setClipboard(const QString &text)
{
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
    QGuiApplication::clipboard()->setText(text);
}

QString readClipboard()
{
    if (!TextInjector::haveWlClipboard())
        return QGuiApplication::clipboard()->text();
    QProcess p;
    p.start(QStringLiteral("wl-paste"), {QStringLiteral("--no-newline")});
    if (!p.waitForFinished(500) || p.exitCode() != 0)
        return QString();
    const QByteArray data = p.readAllStandardOutput();
    if (data.size() > 1'000'000) // don't try to restore huge payloads
        return QString();
    return QString::fromUtf8(data);
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
    }

    Settings *m_settings;
    PortalRemoteDesktop *m_portalRd;
};

} // namespace

// ---------------------------------------------------------------------------

bool TextInjector::ydotoolReady()
{
    if (QStandardPaths::findExecutable(QStringLiteral("ydotool")).isEmpty())
        return false;
    const QString socket = qEnvironmentVariable(
        "YDOTOOL_SOCKET",
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
            + QStringLiteral("/.ydotool_socket"));
    return QFile::exists(socket);
}

bool TextInjector::haveWlClipboard()
{
    return !QStandardPaths::findExecutable(QStringLiteral("wl-copy")).isEmpty();
}

QString TextInjector::diagnostics()
{
    QStringList found, missing;
    (haveWlClipboard() ? found : missing) << QStringLiteral("wl-clipboard");
    (ydotoolReady() ? found : missing) << QStringLiteral("ydotool");
    (PortalRemoteDesktop::available() ? found : missing) << QStringLiteral("RemoteDesktop portal");
    QString s = tr("Detected: %1").arg(found.isEmpty() ? tr("nothing") : found.join(QStringLiteral(", ")));
    if (!missing.isEmpty())
        s += tr(" — missing: %1").arg(missing.join(QStringLiteral(", ")));
    return s;
}

TextInjector *TextInjector::create(Settings *settings, PortalRemoteDesktop *portalRd,
                                   QObject *parent)
{
    const QString mode = settings->injectionMode();

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
}
