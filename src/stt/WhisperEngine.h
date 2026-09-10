#pragma once

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

struct whisper_context;

// Thin wrapper around whisper.cpp. Lives on a dedicated worker thread (queued
// slot invocations serialize the work), so a decode never blocks the UI. One
// request = one whisper_full() run over the given audio.
//
// ── Why there is a cancel, and why it is an atomic ───────────────────────────
// Requests are serialized FIFO by the worker's event loop, and a partial decode
// of a 20-second utterance on a CPU-only build can take longer than the audio
// it is decoding. When the speaker stops, the *final* decode of that utterance
// is queued behind a partial whose result will be thrown away — and on a slow
// machine behind several of them. That is most of what "stuck on formatting for
// ages" was.
//
// `dropPartialsBefore()` is called from the GUI thread and must take effect
// while the worker is *inside* whisper_full(), so it cannot be a queued slot —
// the worker would not read it until the decode it is meant to stop had already
// finished. It is a relaxed atomic read by ggml's abort callback instead.
// Finals are never dropped; only partials, whose output is a preview.
class WhisperEngine : public QObject
{
    Q_OBJECT
public:
    explicit WhisperEngine(QObject *parent = nullptr);
    ~WhisperEngine() override;

    /** Abandon every partial request with an id below `id`, queued or running.
     *  Thread-safe by design — see the note above. */
    void dropPartialsBefore(quint64 id);

public slots:
    void loadModel(const QString &path);
    void unload();
    // finalPass=true enables temperature fallback for better quality on the
    // committed decode; partials use the fastest settings.
    void transcribe(quint64 id, const QVector<float> &audio, const QString &language,
                    const QString &prompt, bool finalPass);

signals:
    void modelLoaded(bool ok, const QString &systemInfo);
    // `elapsedMs` is what this decode actually cost. The session uses it to stop
    // asking for partials it cannot afford.
    void transcribed(quint64 id, const QString &text, qint64 elapsedMs);
    // 0-100 through a final decode, so a long CPU pass is not a frozen label.
    void decodeProgress(quint64 id, int percent);

private:
    bool shouldAbortCurrent() const;

    whisper_context *m_ctx = nullptr;
    QString m_path;

    std::atomic<quint64> m_dropPartialsBefore{0};
    // Worker-thread only, read by the ggml callbacks.
    quint64 m_currentId = 0;
    bool m_currentIsFinal = false;
};
