#include "stt/WhisperEngine.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#include <whisper.h>

#include <algorithm>
#include <thread>

namespace {
/** How many threads a decode gets.
 *
 *  Was `min(8, max(2, hardware_concurrency()))`. Eight is a reasonable ceiling
 *  on a small machine and a waste on a 24-thread one, which is exactly the
 *  machine where a CPU-only build is slowest in wall-clock terms and most needs
 *  the help. Two are left for the GUI and the audio callback: whisper saturates
 *  every thread it is given, and a decode that starves the render thread is a
 *  frozen window whatever the state label says. Sixteen is where whisper.cpp's
 *  own scaling flattens out.
 */
int decodeThreads()
{
    const unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0)
        return 4;
    return int(std::clamp<unsigned>(hw - 2, 2u, 16u));
}
} // namespace

WhisperEngine::WhisperEngine(QObject *parent)
    : QObject(parent)
{
    // Route ggml/whisper logs away from stderr spam; keep warnings.
    whisper_log_set(
        [](ggml_log_level level, const char *text, void *) {
            if (level >= GGML_LOG_LEVEL_WARN)
                qDebug().noquote() << "[whisper]" << QString::fromUtf8(text).trimmed();
        },
        nullptr);
}

WhisperEngine::~WhisperEngine()
{
    unload();
}

void WhisperEngine::dropPartialsBefore(quint64 id)
{
    // Monotonic: a later call must never lower the bar.
    quint64 prev = m_dropPartialsBefore.load(std::memory_order_relaxed);
    while (prev < id
           && !m_dropPartialsBefore.compare_exchange_weak(prev, id, std::memory_order_relaxed))
        ;
}

bool WhisperEngine::shouldAbortCurrent() const
{
    return !m_currentIsFinal
        && m_currentId < m_dropPartialsBefore.load(std::memory_order_relaxed);
}

void WhisperEngine::unload()
{
    if (m_ctx) {
        whisper_free(m_ctx);
        m_ctx = nullptr;
        m_path.clear();
    }
}

void WhisperEngine::loadModel(const QString &path)
{
    if (m_ctx && m_path == path) {
        emit modelLoaded(true, QString::fromUtf8(whisper_print_system_info()));
        return;
    }
    unload();

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;

    m_ctx = whisper_init_from_file_with_params(path.toUtf8().constData(), cparams);
    if (!m_ctx) {
        emit modelLoaded(false, tr("Failed to load model %1").arg(path));
        return;
    }
    m_path = path;
    emit modelLoaded(true, QString::fromUtf8(whisper_print_system_info()));
}

void WhisperEngine::transcribe(quint64 id, const QVector<float> &audio, const QString &language,
                               const QString &prompt, bool finalPass)
{
    if (!m_ctx) {
        emit transcribed(id, QString(), 0);
        return;
    }

    // Queued behind something that has since made it pointless: the utterance
    // was committed, or the recording ended, while this partial waited its turn.
    if (!finalPass && id < m_dropPartialsBefore.load(std::memory_order_relaxed)) {
        emit transcribed(id, QString(), 0);
        return;
    }

    m_currentId = id;
    m_currentIsFinal = finalPass;

    // whisper_full needs at least ~1 s of audio; pad with silence.
    QVector<float> samples = audio;
    const int minSamples = int(WHISPER_SAMPLE_RATE * 1.25);
    if (samples.size() < minSamples)
        samples.resize(minSamples);

    const QByteArray lang = language.isEmpty() ? QByteArrayLiteral("auto") : language.toUtf8();
    const QByteArray promptUtf8 = prompt.toUtf8();

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.print_realtime = false;
    p.print_progress = false;
    p.print_timestamps = false;
    p.print_special = false;
    p.no_timestamps = true;
    p.single_segment = false;
    p.no_context = true; // context is provided explicitly via initial_prompt
    p.suppress_blank = true;
    p.suppress_nst = true;
    p.language = lang.constData();
    p.initial_prompt = promptUtf8.isEmpty() ? nullptr : promptUtf8.constData();
    p.n_threads = decodeThreads();
    p.temperature = 0.0f;
    p.temperature_inc = finalPass ? 0.2f : 0.0f; // partials: no fallback retries
    p.greedy.best_of = finalPass ? 2 : 1;

    // The two ggml callbacks. Both run on this thread, inside whisper_full().
    p.abort_callback = [](void *user) -> bool {
        return static_cast<WhisperEngine *>(user)->shouldAbortCurrent();
    };
    p.abort_callback_user_data = this;
    if (finalPass) {
        p.progress_callback = [](whisper_context *, whisper_state *, int progress, void *user) {
            auto *self = static_cast<WhisperEngine *>(user);
            emit self->decodeProgress(self->m_currentId, progress);
        };
        p.progress_callback_user_data = this;
    }

    QElapsedTimer clock;
    clock.start();

    QString text;
    const int rc = whisper_full(m_ctx, p, samples.constData(), samples.size());
    const qint64 elapsed = clock.elapsed();

    if (shouldAbortCurrent()) {
        // Abandoned mid-decode. Whatever whisper had is a fragment of a preview
        // nobody is waiting for any more.
        emit transcribed(id, QString(), elapsed);
        m_currentId = 0;
        return;
    }

    if (rc == 0) {
        const int n = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < n; ++i)
            text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    } else {
        qWarning() << "whisper_full failed for request" << id << "rc" << rc;
    }

    m_currentId = 0;
    emit transcribed(id, text.trimmed(), elapsed);
}
