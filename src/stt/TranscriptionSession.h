#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QVector>

#include "format/TextFormatter.h"
#include "stt/SpeechGate.h"

class Settings;

// Orchestrates one recording: consumes 16 kHz mono audio, segments it into
// utterances with the SpeechGate, requests live partial decodes plus a
// final decode per utterance from the WhisperEngine (worker thread), and
// assembles the formatted result when the recording ends.
//
// Timing runs on the audio sample clock, not wall time, so behaviour is
// deterministic and testable.
class TranscriptionSession : public QObject
{
    Q_OBJECT
public:
    explicit TranscriptionSession(Settings *settings, QObject *parent = nullptr);

    void begin();
    void feed(const QVector<float> &chunk);
    void end();   // finish: finished() fires once outstanding decodes land
    void abort(); // discard everything, go idle immediately

    bool isActive() const { return m_phase != Phase::Inactive; }
    bool isDraining() const { return m_phase == Phase::Draining; }

signals:
    // Connected to WhisperEngine::transcribe with a queued connection.
    void requestTranscribe(quint64 id, const QVector<float> &audio, const QString &language,
                           const QString &prompt, bool finalPass);
    void partialTextChanged(const QString &displayText);
    void finished(const QString &formattedText);

public slots:
    void onTranscribed(quint64 id, const QString &text);

private:
    enum class Phase { Inactive, Recording, Draining };

    struct Utterance {
        QString text;
        qint64 pauseBeforeMs = 0;
        bool decoded = false;
    };

    void processWindow(const float *data, int count);
    void maybeRequestPartial();
    void finalizeUtterance(bool force);
    void assembleIfDone();
    void emitPartial();
    QString promptContext() const;
    TextFormatter::Options formatOptions() const;
    qint64 nowMs() const { return qint64(m_samplesSeen * 1000 / 16000); }

    Settings *m_settings;
    Phase m_phase = Phase::Inactive;
    SpeechGate m_gate;

    // windowing for RMS analysis (30 ms)
    static constexpr int kWindow = 480;
    QVector<float> m_windowBuf;

    QVector<float> m_preroll;   // rolling ~300 ms kept while silent
    QVector<float> m_utterance; // audio of the utterance in progress
    bool m_inUtterance = false;
    qint64 m_utteranceStartMs = 0;
    qint64 m_lastUtteranceEndMs = -1;
    qint64 m_samplesSeen = 0;

    QList<Utterance> m_committed;
    QHash<quint64, int> m_finalRequests; // request id -> index into m_committed
    quint64 m_nextRequestId = 1;
    quint64 m_partialRequestId = 0; // 0 = none in flight
    qint64 m_lastPartialMs = 0;
    QString m_livePartial;
};
