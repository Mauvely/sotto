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
    /** Give up on the decodes still outstanding and assemble what has landed.
     *  The escape hatch from a long CPU drain — the alternative was a HUD that
     *  said "Formatting…" until whisper was done, with no way to stop it. */
    void finishNow();

    bool isActive() const { return m_phase != Phase::Inactive; }
    bool isDraining() const { return m_phase == Phase::Draining; }
    /** Final decodes still outstanding. What the UI counts down while draining. */
    int pendingDecodes() const { return int(m_finalRequests.size()); }

signals:
    // Connected to WhisperEngine::transcribe with a queued connection.
    void requestTranscribe(quint64 id, const QVector<float> &audio, const QString &language,
                           const QString &prompt, bool finalPass);
    /** Every partial request older than `beforeId` is now pointless. Wired to
     *  WhisperEngine::dropPartialsBefore() with a **direct** connection: the
     *  worker may be inside whisper_full() on one of them, and a queued call
     *  would not be read until that decode had finished — which is the wait it
     *  exists to cut short. */
    void dropStalePartials(quint64 beforeId);
    void partialTextChanged(const QString &displayText);
    void pendingDecodesChanged();
    void finished(const QString &formattedText);

public slots:
    void onTranscribed(quint64 id, const QString &text, qint64 elapsedMs);

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
    /** What the last partial decode actually cost, in wall-clock ms. On a
     *  CPU-only build it can exceed the audio it decoded, and asking for the
     *  next one on the configured cadence regardless is how the worker ends up
     *  permanently one decode behind — with every *final* queued behind a
     *  preview. The next partial waits at least this long instead. */
    qint64 m_lastPartialCostMs = 0;
    QString m_livePartial;
};
