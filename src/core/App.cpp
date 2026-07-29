#include "core/App.h"

#include "audio/AudioCapture.h"
#include "core/Notifier.h"
#include "core/Settings.h"
#include "hotkey/GlobalShortcutsPortal.h"
#include "inject/PortalRemoteDesktop.h"
#include "inject/TextInjector.h"
#include "portal/PortalRequest.h"
#include "stt/ModelManager.h"
#include "stt/TranscriptionSession.h"
#include "stt/WhisperEngine.h"
#include "ui/OverlayController.h"
#include "ui/TrayIcon.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>

namespace {
constexpr int kLevelBars = 22;
}

App::App(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_levels(kLevelBars, 0.0f)
{
}

App::~App()
{
    m_whisperThread.quit();
    // A CPU decode of a long utterance can take a while; killing the thread
    // mid-inference would crash on exit.
    m_whisperThread.wait(15000);
}

void App::initialize()
{
    // Portals >= 1.20 require unsandboxed apps to self-report an app id
    // before using identity-sensitive interfaces like GlobalShortcuts.
    // Must happen before GlobalShortcutsPortal/PortalRemoteDesktop below.
    Portal::registerHostApp(QStringLiteral("net.mauvely.sotto.app"));

    m_models = new ModelManager(this);
    m_capture = new AudioCapture(this);
    m_session = new TranscriptionSession(m_settings, this);
    m_portalRd = new PortalRemoteDesktop(m_settings, this);

    // Whisper worker thread.
    m_whisper = new WhisperEngine;
    m_whisper->moveToThread(&m_whisperThread);
    connect(&m_whisperThread, &QThread::finished, m_whisper, &QObject::deleteLater);
    m_whisperThread.setObjectName(QStringLiteral("whisper"));
    m_whisperThread.start();

    connect(this, &App::loadModelRequested, m_whisper, &WhisperEngine::loadModel);
    connect(m_whisper, &WhisperEngine::modelLoaded, this, &App::onModelLoaded);
    connect(m_session, &TranscriptionSession::requestTranscribe, m_whisper, &WhisperEngine::transcribe);
    connect(m_whisper, &WhisperEngine::transcribed, m_session, &TranscriptionSession::onTranscribed);

    // Audio -> session + visualiser.
    connect(m_capture, &AudioCapture::samples, m_session, &TranscriptionSession::feed);
    connect(m_capture, &AudioCapture::level, this, [this](float v) {
        m_levels.removeFirst();
        m_levels.append(v);
        emit levelsChanged();
    });
    connect(m_capture, &AudioCapture::errorOccurred, this, &App::onCaptureError);

    connect(m_session, &TranscriptionSession::partialTextChanged, this, [this](const QString &t) {
        m_partialText = t;
        emit partialTextChanged();
    });
    connect(m_session, &TranscriptionSession::finished, this, &App::onSessionFinished);

    // QML engine + windows.
    m_engine = new QQmlEngine(this);
    m_engine->rootContext()->setContextProperty(QStringLiteral("App"), this);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Config"), m_settings);
    m_engine->rootContext()->setContextProperty(QStringLiteral("Models"), m_models);

    m_overlay = new OverlayController(m_engine, m_settings, this);
    m_overlay->initialize();

    m_tray = new TrayIcon(this, this);

    // Global shortcut via the portal.
    m_hotkey = new GlobalShortcutsPortal(this);
    connect(m_hotkey, &GlobalShortcutsPortal::activated, this, [this] {
        if (m_settings->hotkeyMode() == QStringLiteral("hold")) {
            if (m_state == State::Idle)
                startDictation(int(Target::Inject));
        } else {
            toggle();
        }
    });
    connect(m_hotkey, &GlobalShortcutsPortal::deactivated, this, [this] {
        if (m_settings->hotkeyMode() == QStringLiteral("hold"))
            stopDictation();
    });
    connect(m_hotkey, &GlobalShortcutsPortal::boundChanged, this, &App::boundShortcutChanged);
    connect(m_hotkey, &GlobalShortcutsPortal::failed, this, [this](const QString &msg) {
        setError(msg);
    });
    applyShortcutSettings();

    connect(m_settings, &Settings::injectionModeChanged, this, &App::injectionDiagnosticsChanged);
}

// ---------------------------------------------------------------------------
// State machine

QString App::stateName() const
{
    switch (m_state) {
    case State::Idle: return QStringLiteral("idle");
    case State::Loading: return QStringLiteral("loading");
    case State::Listening: return QStringLiteral("listening");
    case State::Finalizing: return QStringLiteral("finalizing");
    case State::Inserting: return QStringLiteral("inserting");
    }
    return QStringLiteral("idle");
}

void App::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    if (s == State::Idle || s == State::Loading) {
        m_levels.fill(0.0f);
        emit levelsChanged();
    }
    emit stateChanged();
}

void App::setError(const QString &message)
{
    m_lastError = message;
    emit lastErrorChanged();
    if (!message.isEmpty())
        Notifier::notify(QStringLiteral("Sotto"), message);
}

void App::toggle()
{
    if (m_state == State::Idle)
        startDictation(int(Target::Inject));
    else if (m_state == State::Listening)
        stopDictation();
    // Loading/finalizing/inserting: ignore rather than queue surprises.
}

void App::startDictation(int target)
{
    if (m_state != State::Idle)
        return;

    const QString modelId = m_settings->modelId();
    if (modelId.isEmpty() || !m_models->isInstalled(modelId)) {
        setError(tr("No speech model installed yet — pick one in Settings."));
        showSettings();
        return;
    }

    m_target = Target(target);
    m_startPending = true;

    if (m_loadedModelId == modelId) {
        beginListening();
        return;
    }
    setState(State::Loading);
    emit loadModelRequested(m_models->pathFor(modelId));
}

void App::onModelLoaded(bool ok, const QString &info)
{
    if (!ok) {
        m_loadedModelId.clear();
        m_startPending = false;
        setState(State::Idle);
        setError(tr("Could not load the speech model: %1").arg(info));
        return;
    }
    m_loadedModelId = m_settings->modelId();
    m_systemInfo = info;
    emit systemInfoChanged();
    if (m_startPending)
        beginListening();
}

void App::beginListening()
{
    m_startPending = false;
    m_session->begin();
    if (!m_capture->start(m_settings->audioDevice())) {
        m_session->abort();
        setState(State::Idle);
        return; // onCaptureError already reported the reason
    }
    setState(State::Listening);
}

void App::stopDictation()
{
    if (m_state == State::Loading) {
        m_startPending = false;
        setState(State::Idle);
        return;
    }
    if (m_state != State::Listening)
        return;
    m_capture->stop();
    setState(State::Finalizing);
    m_session->end();
}

void App::onSessionFinished(const QString &text)
{
    if (text.isEmpty()) {
        setState(State::Idle);
        return;
    }

    if (m_target == Target::Notepad) {
        setNotepadText(m_notepadText.isEmpty() ? text
                                               : m_notepadText + QStringLiteral("\n\n") + text);
        setState(State::Idle);
        return;
    }

    setState(State::Inserting);
    auto *injector = TextInjector::create(m_settings, m_portalRd, this);
    connect(injector, &TextInjector::finished, this,
            [this, injector](bool ok, const QString &message) {
                injector->deleteLater();
                setState(State::Idle);
                if (!message.isEmpty())
                    Notifier::notify(QStringLiteral("Sotto"), message);
                else if (!ok)
                    Notifier::notify(QStringLiteral("Sotto"), tr("Inserting the text failed."));
            });
    injector->inject(text);
}

void App::onCaptureError(const QString &message)
{
    setError(message);
    if (m_state == State::Listening) {
        m_capture->stop();
        m_session->abort();
        setState(State::Idle);
    }
}

// ---------------------------------------------------------------------------
// Windows

QQuickWindow *App::ensureWindow(QPointer<QQuickWindow> &slot, const QString &qmlFile)
{
    if (!slot) {
        QQmlComponent component(m_engine,
                                QUrl(QStringLiteral("qrc:/qt/qml/Sotto/qml/") + qmlFile));
        QObject *obj = component.create();
        if (!obj) {
            qWarning() << component.errorString();
            return nullptr;
        }
        slot = qobject_cast<QQuickWindow *>(obj);
    }
    return slot;
}

void App::showSettings()
{
    if (QQuickWindow *w = ensureWindow(m_settingsWindow, QStringLiteral("SettingsWindow.qml"))) {
        w->show();
        w->raise();
        w->requestActivate();
    }
}

void App::showNotepad()
{
    if (QQuickWindow *w = ensureWindow(m_notepadWindow, QStringLiteral("NotepadWindow.qml"))) {
        w->show();
        w->raise();
        w->requestActivate();
    }
}

void App::quit()
{
    // Windows must go before the engine (a child of this object) does,
    // otherwise their bindings evaluate against dead context properties.
    if (m_overlay)
        m_overlay->destroyWindow();
    delete m_settingsWindow.data();
    delete m_notepadWindow.data();
    QCoreApplication::quit();
}

// ---------------------------------------------------------------------------
// QML helpers

void App::toggleNotepadDictation()
{
    if (m_state == State::Idle)
        startDictation(int(Target::Notepad));
    else if (m_state == State::Listening)
        stopDictation();
}

void App::applyShortcutSettings()
{
    if (!m_hotkey)
        return;
    if (!m_settings->shortcutEnabled()) {
        m_hotkey->release();
        return;
    }
    if (!GlobalShortcutsPortal::available()) {
        setError(tr("The GlobalShortcuts portal is not available; use the tray icon or "
                    "`sotto --toggle` bound to a compositor shortcut instead."));
        return;
    }
    m_hotkey->bind(m_settings->preferredShortcut());
}

void App::copyToClipboard(const QString &text)
{
    QGuiApplication::clipboard()->setText(text);
}

void App::setNotepadText(const QString &t)
{
    if (m_notepadText == t)
        return;
    m_notepadText = t;
    emit notepadTextChanged();
}

QString App::boundShortcut() const
{
    return m_hotkey ? m_hotkey->triggerDescription() : QString();
}

QVariantList App::levels() const
{
    QVariantList out;
    out.reserve(m_levels.size());
    for (float v : m_levels)
        out.append(double(v));
    return out;
}

QStringList App::audioDevices() const
{
    return AudioCapture::availableDevices();
}

void App::refreshAudioDevices()
{
    emit audioDevicesChanged();
}

QStringList App::screenNames() const
{
    QStringList out{QStringLiteral("auto")};
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
        out << s->name();
    return out;
}

QString App::injectionDiagnostics() const
{
    return TextInjector::diagnostics();
}
