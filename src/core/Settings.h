#pragma once

#include <QObject>
#include <QSettings>

// Persistent user configuration, backed by an INI file at
// ~/.config/sotto/sotto.conf and exposed to QML as the "Config"
// context property. Every value is read/written through QSettings on
// access so external edits are picked up on restart.
class Settings : public QObject
{
    Q_OBJECT

    // Transcription
    Q_PROPERTY(QString modelId READ modelId WRITE setModelId NOTIFY modelIdChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    // Global shortcut
    Q_PROPERTY(bool shortcutEnabled READ shortcutEnabled WRITE setShortcutEnabled NOTIFY shortcutEnabledChanged)
    Q_PROPERTY(QString preferredShortcut READ preferredShortcut WRITE setPreferredShortcut NOTIFY preferredShortcutChanged)
    Q_PROPERTY(QString hotkeyMode READ hotkeyMode WRITE setHotkeyMode NOTIFY hotkeyModeChanged) // "toggle" | "hold"

    // Output / injection
    Q_PROPERTY(QString injectionMode READ injectionMode WRITE setInjectionMode NOTIFY injectionModeChanged) // "auto" | "clipboard-paste" | "ydotool-type" | "clipboard-only"
    Q_PROPERTY(bool restoreClipboard READ restoreClipboard WRITE setRestoreClipboard NOTIFY restoreClipboardChanged)

    // Formatting
    Q_PROPERTY(double paragraphPauseSec READ paragraphPauseSec WRITE setParagraphPauseSec NOTIFY paragraphPauseSecChanged)
    Q_PROPERTY(bool voiceCommands READ voiceCommands WRITE setVoiceCommands NOTIFY voiceCommandsChanged)
    Q_PROPERTY(bool voiceCmdNewLine READ voiceCmdNewLine WRITE setVoiceCmdNewLine NOTIFY voiceCmdNewLineChanged)
    Q_PROPERTY(bool voiceCmdNewParagraph READ voiceCmdNewParagraph WRITE setVoiceCmdNewParagraph NOTIFY voiceCmdNewParagraphChanged)
    Q_PROPERTY(bool voiceCmdDeleteLastLine READ voiceCmdDeleteLastLine WRITE setVoiceCmdDeleteLastLine NOTIFY voiceCmdDeleteLastLineChanged)
    Q_PROPERTY(bool voiceCmdDeleteLastSentence READ voiceCmdDeleteLastSentence WRITE setVoiceCmdDeleteLastSentence NOTIFY voiceCmdDeleteLastSentenceChanged)

    // Appearance
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(bool overlayTranslucent READ overlayTranslucent WRITE setOverlayTranslucent NOTIFY overlayTranslucentChanged)
    Q_PROPERTY(QString overlayScreen READ overlayScreen WRITE setOverlayScreen NOTIFY overlayScreenChanged) // "auto" or a QScreen name

    // Audio
    Q_PROPERTY(QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioDeviceChanged) // "default" or device description

    // General
    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY launchAtLoginChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    QString modelId() const;
    void setModelId(const QString &v);
    QString language() const;
    void setLanguage(const QString &v);

    bool shortcutEnabled() const;
    void setShortcutEnabled(bool v);
    QString preferredShortcut() const;
    void setPreferredShortcut(const QString &v);
    QString hotkeyMode() const;
    void setHotkeyMode(const QString &v);

    QString injectionMode() const;
    void setInjectionMode(const QString &v);
    bool restoreClipboard() const;
    void setRestoreClipboard(bool v);

    double paragraphPauseSec() const;
    void setParagraphPauseSec(double v);
    bool voiceCommands() const;
    void setVoiceCommands(bool v);
    bool voiceCmdNewLine() const;
    void setVoiceCmdNewLine(bool v);
    bool voiceCmdNewParagraph() const;
    void setVoiceCmdNewParagraph(bool v);
    bool voiceCmdDeleteLastLine() const;
    void setVoiceCmdDeleteLastLine(bool v);
    bool voiceCmdDeleteLastSentence() const;
    void setVoiceCmdDeleteLastSentence(bool v);

    bool animationsEnabled() const;
    void setAnimationsEnabled(bool v);
    bool overlayTranslucent() const;
    void setOverlayTranslucent(bool v);
    QString overlayScreen() const;
    void setOverlayScreen(const QString &v);

    QString audioDevice() const;
    void setAudioDevice(const QString &v);

    bool launchAtLogin() const;
    void setLaunchAtLogin(bool v);

    // Tuning knobs without UI; editable in sotto.conf.
    int silenceMs() const;          // pause that ends an utterance
    int minUtteranceMs() const;     // shorter utterances are discarded as noise
    int partialIntervalMs() const;  // live re-decode cadence
    int maxUtteranceSec() const;    // hard cap before force-committing
    double voiceThreshold() const;  // absolute RMS floor considered "speech"

    // Opaque token handed back by the RemoteDesktop portal so the
    // permission dialog is only shown once.
    QString remoteDesktopRestoreToken() const;
    void setRemoteDesktopRestoreToken(const QString &token);

    bool firstRun() const;
    void setFirstRunDone();

signals:
    void modelIdChanged();
    void languageChanged();
    void shortcutEnabledChanged();
    void preferredShortcutChanged();
    void hotkeyModeChanged();
    void injectionModeChanged();
    void restoreClipboardChanged();
    void paragraphPauseSecChanged();
    void voiceCommandsChanged();
    void voiceCmdNewLineChanged();
    void voiceCmdNewParagraphChanged();
    void voiceCmdDeleteLastLineChanged();
    void voiceCmdDeleteLastSentenceChanged();
    void animationsEnabledChanged();
    void overlayTranslucentChanged();
    void overlayScreenChanged();
    void audioDeviceChanged();
    void launchAtLoginChanged();

private:
    QString autostartFilePath() const;
    mutable QSettings m_s;
};
