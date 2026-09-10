#include "core/App.h"

#include "audio/AudioCapture.h"
#include "core/Notifier.h"
#include "core/Settings.h"
#include "hotkey/HotkeyBackend.h"
#include "inject/TextInjector.h"
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
#include <QtMath>

#include <cmath>

#ifndef Q_OS_WIN
#include "inject/PortalRemoteDesktop.h"
#include "portal/PortalRequest.h"
#endif

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
#ifndef Q_OS_WIN
    // Portals >= 1.20 require unsandboxed apps to self-report an app id
    // before using identity-sensitive interfaces like GlobalShortcuts.
    // Must happen before GlobalShortcutsPortal/PortalRemoteDesktop below.
    Portal::registerHostApp(QStringLiteral("net.mauvely.sotto.app"));
#endif

    m_models = new ModelManager(this);
    m_capture = new AudioCapture(this);
    m_session = new TranscriptionSession(m_settings, this);
#ifndef Q_OS_WIN
    m_portalRd = new PortalRemoteDesktop(m_settings, this);
#endif

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

    // **Direct**, and deliberately so. The worker may be inside whisper_full()
    // on the very partial this cancels, and a queued call would only be read
    // after that decode finished — which is the wait it exists to cut short.
    // dropPartialsBefore() is an atomic store for exactly this reason.
    connect(
        m_session, &TranscriptionSession::dropStalePartials, m_whisper,
        [this](quint64 beforeId) { m_whisper->dropPartialsBefore(beforeId); },
        Qt::DirectConnection);

    connect(m_session, &TranscriptionSession::pendingDecodesChanged, this, [this] {
        m_pendingDecodes = m_session->pendingDecodes();
        m_decodePercent = -1;
        emit statusTextChanged();
    });
    connect(m_whisper, &WhisperEngine::decodeProgress, this, [this](quint64, int percent) {
        if (m_state != State::Finalizing)
            return;
        m_decodePercent = percent;
        emit statusTextChanged();
    });

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

    // Global shortcut (portal on Linux, RegisterHotKey on Windows).
    m_hotkey = new HotkeyBackend(this);
    connect(m_hotkey, &HotkeyBackend::activated, this, [this] {
        if (m_settings->hotkeyMode() == QStringLiteral("hold")) {
            if (m_state == State::Idle)
                startDictation(int(Target::Inject));
        } else {
            toggle();
        }
    });
    connect(m_hotkey, &HotkeyBackend::deactivated, this, [this] {
        if (m_settings->hotkeyMode() == QStringLiteral("hold"))
            stopDictation();
    });
    connect(m_hotkey, &HotkeyBackend::boundChanged, this, &App::boundShortcutChanged);
    connect(m_hotkey, &HotkeyBackend::failed, this, [this](const QString &msg) {
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
    if (s != State::Finalizing)
        m_decodePercent = -1;
    emit stateChanged();
    emit statusTextChanged();
}

// What the HUD, the notepad's transport strip and the tray tooltip all say.
//
// The old text called the whole Finalizing state "Formatting…", which is where
// this went wrong: formatting is TextFormatter::format(), a few regular
// expressions over a few kilobytes, and it has never taken a measurable amount
// of time. What Finalizing actually waits for is whisper decoding the utterances
// that were committed while the person was still speaking — minutes of it, on a
// CPU-only build with a large model. A label that names the cheap step and hides
// the expensive one is how "stuck on formatting" became the report.
QString App::statusText() const
{
    switch (m_state) {
    case State::Idle:
        return QString();
    case State::Loading:
        return tr("Loading the model…");
    case State::Listening:
        return tr("Listening…");
    case State::Inserting:
        return tr("Inserting…");
    case State::Finalizing:
        // Not tr("…%n passage(s) left", nullptr, n): the (s) plural markup is a
        // Qt Linguist convention that only a loaded translation resolves, and
        // Sotto has none yet — it rendered literally as "2 passage(s) left".
        // This branch only runs for n > 1, so the plural is unconditional.
        // Short because the HUD is a 440px pill and this shares it with the
        // logo, the level bars and the LOCAL badge — about 180px of room.
        if (m_pendingDecodes > 1)
            return tr("Transcribing… %1 left").arg(m_pendingDecodes);
        if (m_pendingDecodes == 1)
            return m_decodePercent >= 0 ? tr("Transcribing… %1%").arg(m_decodePercent)
                                        : tr("Transcribing…");
        return tr("Formatting…");
    }
    return QString();
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
    else if (m_state == State::Finalizing)
        // The escape hatch. A CPU decode of a long dictation can run for
        // minutes, and pressing the shortcut again used to do nothing at all —
        // the app looked hung, with no way out but killing it. A second press
        // now takes what has decoded and stops waiting for the rest.
        m_session->finishNow();
    // Loading/inserting: ignore rather than queue surprises.
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

QQuickWindow *App::harnessWindow(const QString &view)
{
    if (view == QLatin1String("notepad")) {
        setNotepadText(tr("Sotto types what you say into whatever has focus, or into "
                          "this scratch buffer when you would rather keep it.\n\n"
                          "Nothing here has been anywhere near a network."));
        return ensureWindow(m_notepadWindow, QStringLiteral("NotepadWindow.qml"));
    }
    if (view == QLatin1String("overlay")) {
        m_partialText = tr("the quick brown fox jumps over the lazy dog");
        emit partialTextChanged();
        // A plausible waveform rather than a flat line, so the bar shape and
        // the elision of the text beside it are both visible.
        for (int i = 0; i < m_levels.size(); ++i)
            m_levels[i] = 0.12f + 0.80f * float(qFabs(std::sin(i * 0.8)));
        emit levelsChanged();
        // SOTTO_STATE=finalizing renders the state a person waits in, which is
        // the one that had nothing to look at. A previous session tried a
        // harness state variable and dropped it because the HUD only drew while
        // listening; it draws a countdown now, so there is something to check.
        const QString state = qEnvironmentVariable("SOTTO_STATE");
        if (state == QLatin1String("finalizing")) {
            m_pendingDecodes = 2;
            setState(State::Finalizing);
        } else {
            setState(State::Listening); // the HUD shows itself when state leaves idle
        }
        return m_overlay ? m_overlay->window() : nullptr;
    }
    return ensureWindow(m_settingsWindow, QStringLiteral("SettingsWindow.qml"));
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
    if (!HotkeyBackend::available()) {
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

// ---------------------------------------------------------------------------
// The backend line
//
// `SOTTO_GPU_BACKEND` is the build's answer and `SOTTO_GPU_REASON` is why —
// both baked in by CMakeLists.txt, which is the only place that knows whether a
// toolchain was found. A user with a discrete GPU reading "cpu" will otherwise
// assume the app failed to *find* the card at run time; the answer is always
// build time, and an artifact built against a GPU backend will not start on a
// machine without that runtime, which is why the shipped build is cpu.

QString App::gpuBackendLabel() const
{
    const QString key = gpuBackend();
    if (key == QLatin1String("cpu"))
        return tr("CPU");
    if (key == QLatin1String("cuda"))
        return tr("CUDA (NVIDIA)");
    if (key == QLatin1String("vulkan"))
        return tr("Vulkan");
    if (key == QLatin1String("hip"))
        return tr("HIP (AMD ROCm)");
    return key;
}

QString App::gpuBackendReason() const
{
    return QString::fromUtf8(SOTTO_GPU_REASON);
}
