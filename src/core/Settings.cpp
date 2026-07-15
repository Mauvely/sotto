#include "core/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace {
const QString kModelId = QStringLiteral("modelId");
const QString kLanguage = QStringLiteral("language");
const QString kShortcutEnabled = QStringLiteral("shortcut/enabled");
const QString kPreferredShortcut = QStringLiteral("shortcut/preferredTrigger");
const QString kHotkeyMode = QStringLiteral("shortcut/mode");
const QString kInjectionMode = QStringLiteral("output/injectionMode");
const QString kRestoreClipboard = QStringLiteral("output/restoreClipboard");
const QString kParagraphPause = QStringLiteral("format/paragraphPauseSec");
const QString kVoiceCommands = QStringLiteral("format/voiceCommands");
const QString kVoiceCmdNewLine = QStringLiteral("format/voiceCmdNewLine");
const QString kVoiceCmdNewParagraph = QStringLiteral("format/voiceCmdNewParagraph");
const QString kVoiceCmdDeleteLastLine = QStringLiteral("format/voiceCmdDeleteLastLine");
const QString kVoiceCmdDeleteLastSentence = QStringLiteral("format/voiceCmdDeleteLastSentence");
const QString kAnimations = QStringLiteral("appearance/animations");
const QString kOverlayTranslucent = QStringLiteral("appearance/overlayTranslucent");
const QString kOverlayScreen = QStringLiteral("appearance/overlayScreen");
const QString kAudioDevice = QStringLiteral("audio/inputDevice");
const QString kSilenceMs = QStringLiteral("tuning/silenceMs");
const QString kMinUtteranceMs = QStringLiteral("tuning/minUtteranceMs");
const QString kPartialIntervalMs = QStringLiteral("tuning/partialIntervalMs");
const QString kMaxUtteranceSec = QStringLiteral("tuning/maxUtteranceSec");
const QString kVoiceThreshold = QStringLiteral("tuning/voiceThreshold");
const QString kRestoreToken = QStringLiteral("portal/remoteDesktopRestoreToken");
const QString kFirstRun = QStringLiteral("firstRun");
} // namespace

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_s(QSettings::IniFormat, QSettings::UserScope,
          QStringLiteral("sotto"), QStringLiteral("sotto"))
{
}

QString Settings::modelId() const { return m_s.value(kModelId, QString()).toString(); }
void Settings::setModelId(const QString &v)
{
    if (modelId() == v)
        return;
    m_s.setValue(kModelId, v);
    emit modelIdChanged();
}

QString Settings::language() const { return m_s.value(kLanguage, QStringLiteral("auto")).toString(); }
void Settings::setLanguage(const QString &v)
{
    if (language() == v)
        return;
    m_s.setValue(kLanguage, v);
    emit languageChanged();
}

bool Settings::shortcutEnabled() const { return m_s.value(kShortcutEnabled, true).toBool(); }
void Settings::setShortcutEnabled(bool v)
{
    if (shortcutEnabled() == v)
        return;
    m_s.setValue(kShortcutEnabled, v);
    emit shortcutEnabledChanged();
}

QString Settings::preferredShortcut() const
{
    return m_s.value(kPreferredShortcut, QStringLiteral("LOGO+ALT+d")).toString();
}
void Settings::setPreferredShortcut(const QString &v)
{
    if (preferredShortcut() == v)
        return;
    m_s.setValue(kPreferredShortcut, v);
    emit preferredShortcutChanged();
}

QString Settings::hotkeyMode() const { return m_s.value(kHotkeyMode, QStringLiteral("toggle")).toString(); }
void Settings::setHotkeyMode(const QString &v)
{
    if (hotkeyMode() == v)
        return;
    m_s.setValue(kHotkeyMode, v);
    emit hotkeyModeChanged();
}

QString Settings::injectionMode() const { return m_s.value(kInjectionMode, QStringLiteral("auto")).toString(); }
void Settings::setInjectionMode(const QString &v)
{
    if (injectionMode() == v)
        return;
    m_s.setValue(kInjectionMode, v);
    emit injectionModeChanged();
}

bool Settings::restoreClipboard() const { return m_s.value(kRestoreClipboard, true).toBool(); }
void Settings::setRestoreClipboard(bool v)
{
    if (restoreClipboard() == v)
        return;
    m_s.setValue(kRestoreClipboard, v);
    emit restoreClipboardChanged();
}

double Settings::paragraphPauseSec() const { return m_s.value(kParagraphPause, 2.0).toDouble(); }
void Settings::setParagraphPauseSec(double v)
{
    if (qFuzzyCompare(paragraphPauseSec(), v))
        return;
    m_s.setValue(kParagraphPause, v);
    emit paragraphPauseSecChanged();
}

bool Settings::voiceCommands() const { return m_s.value(kVoiceCommands, true).toBool(); }
void Settings::setVoiceCommands(bool v)
{
    if (voiceCommands() == v)
        return;
    m_s.setValue(kVoiceCommands, v);
    emit voiceCommandsChanged();
}

bool Settings::voiceCmdNewLine() const { return m_s.value(kVoiceCmdNewLine, true).toBool(); }
void Settings::setVoiceCmdNewLine(bool v)
{
    if (voiceCmdNewLine() == v)
        return;
    m_s.setValue(kVoiceCmdNewLine, v);
    emit voiceCmdNewLineChanged();
}

bool Settings::voiceCmdNewParagraph() const { return m_s.value(kVoiceCmdNewParagraph, true).toBool(); }
void Settings::setVoiceCmdNewParagraph(bool v)
{
    if (voiceCmdNewParagraph() == v)
        return;
    m_s.setValue(kVoiceCmdNewParagraph, v);
    emit voiceCmdNewParagraphChanged();
}

bool Settings::voiceCmdDeleteLastLine() const { return m_s.value(kVoiceCmdDeleteLastLine, true).toBool(); }
void Settings::setVoiceCmdDeleteLastLine(bool v)
{
    if (voiceCmdDeleteLastLine() == v)
        return;
    m_s.setValue(kVoiceCmdDeleteLastLine, v);
    emit voiceCmdDeleteLastLineChanged();
}

bool Settings::voiceCmdDeleteLastSentence() const { return m_s.value(kVoiceCmdDeleteLastSentence, true).toBool(); }
void Settings::setVoiceCmdDeleteLastSentence(bool v)
{
    if (voiceCmdDeleteLastSentence() == v)
        return;
    m_s.setValue(kVoiceCmdDeleteLastSentence, v);
    emit voiceCmdDeleteLastSentenceChanged();
}

bool Settings::animationsEnabled() const { return m_s.value(kAnimations, true).toBool(); }
void Settings::setAnimationsEnabled(bool v)
{
    if (animationsEnabled() == v)
        return;
    m_s.setValue(kAnimations, v);
    emit animationsEnabledChanged();
}

bool Settings::overlayTranslucent() const { return m_s.value(kOverlayTranslucent, false).toBool(); }
void Settings::setOverlayTranslucent(bool v)
{
    if (overlayTranslucent() == v)
        return;
    m_s.setValue(kOverlayTranslucent, v);
    emit overlayTranslucentChanged();
}

QString Settings::overlayScreen() const { return m_s.value(kOverlayScreen, QStringLiteral("auto")).toString(); }
void Settings::setOverlayScreen(const QString &v)
{
    if (overlayScreen() == v)
        return;
    m_s.setValue(kOverlayScreen, v);
    emit overlayScreenChanged();
}

QString Settings::audioDevice() const { return m_s.value(kAudioDevice, QStringLiteral("default")).toString(); }
void Settings::setAudioDevice(const QString &v)
{
    if (audioDevice() == v)
        return;
    m_s.setValue(kAudioDevice, v);
    emit audioDeviceChanged();
}

QString Settings::autostartFilePath() const
{
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return configDir + QStringLiteral("/autostart/io.github.timurinal.sotto.desktop");
}

bool Settings::launchAtLogin() const { return QFile::exists(autostartFilePath()); }
void Settings::setLaunchAtLogin(bool v)
{
    if (launchAtLogin() == v)
        return;
    if (v) {
        QDir().mkpath(QFileInfo(autostartFilePath()).absolutePath());
        QFile f(autostartFilePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("[Desktop Entry]\n"
                    "Type=Application\n"
                    "Name=Sotto\n"
                    "Comment=Fully local voice dictation\n"
                    "Exec=" + QCoreApplication::applicationFilePath().toUtf8() + "\n"
                    "Icon=io.github.timurinal.sotto\n"
                    "X-KDE-autostart-phase=2\n");
        }
    } else {
        QFile::remove(autostartFilePath());
    }
    emit launchAtLoginChanged();
}

int Settings::silenceMs() const { return m_s.value(kSilenceMs, 700).toInt(); }
int Settings::minUtteranceMs() const { return m_s.value(kMinUtteranceMs, 300).toInt(); }
int Settings::partialIntervalMs() const { return m_s.value(kPartialIntervalMs, 1100).toInt(); }
int Settings::maxUtteranceSec() const { return m_s.value(kMaxUtteranceSec, 25).toInt(); }
double Settings::voiceThreshold() const { return m_s.value(kVoiceThreshold, 0.006).toDouble(); }

QString Settings::remoteDesktopRestoreToken() const { return m_s.value(kRestoreToken).toString(); }
void Settings::setRemoteDesktopRestoreToken(const QString &token) { m_s.setValue(kRestoreToken, token); }

bool Settings::firstRun() const { return m_s.value(kFirstRun, true).toBool(); }
void Settings::setFirstRunDone() { m_s.setValue(kFirstRun, false); }
