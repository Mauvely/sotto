#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QThread>
#include <QVariantList>
#include <QVector>

class AudioCapture;
class ModelManager;
class OverlayController;
class PortalRemoteDesktop;
class QQmlEngine;
class QQuickWindow;
class Settings;
class TranscriptionSession;
class TrayIcon;
class WhisperEngine;

#ifdef Q_OS_WIN
class WinHotkey;
using HotkeyBackend = WinHotkey;
#else
class GlobalShortcutsPortal;
using HotkeyBackend = GlobalShortcutsPortal;
#endif

// Central wiring + dictation state machine:
//
//   idle -> loading (model) -> listening -> finalizing -> inserting -> idle
//
// Owns the QML engine, overlay/settings/notepad windows, tray icon, audio
// capture, whisper worker thread, portal hotkey and injection.
class App : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ stateName NOTIFY stateChanged)
    Q_PROPERTY(QString partialText READ partialText NOTIFY partialTextChanged)
    Q_PROPERTY(QVariantList levels READ levels NOTIFY levelsChanged)
    Q_PROPERTY(QString notepadText READ notepadText WRITE setNotepadText NOTIFY notepadTextChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString boundShortcut READ boundShortcut NOTIFY boundShortcutChanged)
    Q_PROPERTY(QString systemInfo READ systemInfo NOTIFY systemInfoChanged)
    Q_PROPERTY(QStringList audioDevices READ audioDevices NOTIFY audioDevicesChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames CONSTANT)
    Q_PROPERTY(QString injectionDiagnostics READ injectionDiagnostics NOTIFY injectionDiagnosticsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString gpuBackend READ gpuBackend CONSTANT)
    Q_PROPERTY(bool blurAvailable READ blurAvailable CONSTANT)

public:
    enum class State { Idle, Loading, Listening, Finalizing, Inserting };
    enum class Target { Inject, Notepad };

    explicit App(Settings *settings, QObject *parent = nullptr);
    ~App() override;

    void initialize();

    // ── Screenshot harness (SOTTO_SHOT; see main.cpp) ────────────────────
    // Hands back one of the three windows, ready to grab. `overlay` also
    // drives the HUD into `listening` with sample levels and a line of
    // partial text, because the pill draws nothing at all while idle and
    // the visualiser has no bars until audio has been captured — a render
    // of the untouched HUD is a transparent 440×64 rectangle.
    QQuickWindow *harnessWindow(const QString &view);

    QString stateName() const;
    QString partialText() const { return m_partialText; }
    QVariantList levels() const;
    QString notepadText() const { return m_notepadText; }
    void setNotepadText(const QString &t);
    QString lastError() const { return m_lastError; }
    QString boundShortcut() const;
    QString systemInfo() const { return m_systemInfo; }
    QStringList audioDevices() const;
    QStringList screenNames() const;
    QString injectionDiagnostics() const;
    QString version() const { return QStringLiteral(SOTTO_VERSION); }
    QString gpuBackend() const { return QStringLiteral(SOTTO_GPU_BACKEND); }
    bool blurAvailable() const
    {
#ifdef SOTTO_HAVE_KWINDOWSYSTEM
        return true;
#else
        return false;
#endif
    }

public slots:
    // D-Bus / CLI / tray entry points
    void toggle();
    void stopDictation();
    void showSettings();
    void showNotepad();
    void quit();

    // QML entry points
    void startDictation(int target = 0); // Target enum as int for QML
    void toggleNotepadDictation();
    void applyShortcutSettings();
    void copyToClipboard(const QString &text);
    void refreshAudioDevices();

signals:
    void stateChanged();
    void partialTextChanged();
    void levelsChanged();
    void notepadTextChanged();
    void lastErrorChanged();
    void boundShortcutChanged();
    void systemInfoChanged();
    void audioDevicesChanged();
    void injectionDiagnosticsChanged();

    // Internal: routed to the whisper worker thread.
    void loadModelRequested(const QString &path);

private slots:
    void onModelLoaded(bool ok, const QString &info);
    void onSessionFinished(const QString &text);
    void onCaptureError(const QString &message);

private:
    void setState(State s);
    void setError(const QString &message);
    void beginListening();
    QQuickWindow *ensureWindow(QPointer<QQuickWindow> &slot, const QString &qmlFile);

    Settings *m_settings;
    QQmlEngine *m_engine = nullptr;
    OverlayController *m_overlay = nullptr;
    TrayIcon *m_tray = nullptr;
    ModelManager *m_models = nullptr;
    AudioCapture *m_capture = nullptr;
    TranscriptionSession *m_session = nullptr;
    WhisperEngine *m_whisper = nullptr;
    QThread m_whisperThread;
    HotkeyBackend *m_hotkey = nullptr;
    PortalRemoteDesktop *m_portalRd = nullptr; // Linux-only; null on Windows

    QPointer<QQuickWindow> m_settingsWindow;
    QPointer<QQuickWindow> m_notepadWindow;

    State m_state = State::Idle;
    Target m_target = Target::Inject;
    bool m_startPending = false;
    QString m_loadedModelId;
    QString m_partialText;
    QString m_notepadText;
    QString m_lastError;
    QString m_systemInfo;
    QVector<float> m_levels;
};
