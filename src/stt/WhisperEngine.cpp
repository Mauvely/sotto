#include "stt/WhisperEngine.h"

#include <QDebug>
#include <QThread>

#include <whisper.h>

#include <thread>

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
        emit transcribed(id, QString());
        return;
    }

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
    p.n_threads = std::min(8u, std::max(2u, std::thread::hardware_concurrency()));
    p.temperature = 0.0f;
    p.temperature_inc = finalPass ? 0.2f : 0.0f; // partials: no fallback retries
    p.greedy.best_of = finalPass ? 2 : 1;

    QString text;
    if (whisper_full(m_ctx, p, samples.constData(), samples.size()) == 0) {
        const int n = whisper_full_n_segments(m_ctx);
        for (int i = 0; i < n; ++i)
            text += QString::fromUtf8(whisper_full_get_segment_text(m_ctx, i));
    } else {
        qWarning() << "whisper_full failed for request" << id;
    }

    emit transcribed(id, text.trimmed());
}
