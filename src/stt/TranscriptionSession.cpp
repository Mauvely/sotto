#include "stt/TranscriptionSession.h"

#include "core/Settings.h"

#include <QtMath>

#include <algorithm>
#include <iterator>

TranscriptionSession::TranscriptionSession(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

TextFormatter::Options TranscriptionSession::formatOptions() const
{
    TextFormatter::Options o;
    o.voiceCommands = m_settings->voiceCommands();
    o.cmdNewLine = m_settings->voiceCmdNewLine();
    o.cmdNewParagraph = m_settings->voiceCmdNewParagraph();
    o.cmdDeleteLastLine = m_settings->voiceCmdDeleteLastLine();
    o.cmdDeleteLastSentence = m_settings->voiceCmdDeleteLastSentence();
    o.paragraphPauseSec = m_settings->paragraphPauseSec();
    return o;
}

void TranscriptionSession::begin()
{
    m_phase = Phase::Recording;
    m_gate = SpeechGate({m_settings->voiceThreshold(), 3.0, 240});
    m_windowBuf.clear();
    m_preroll.clear();
    m_utterance.clear();
    m_inUtterance = false;
    m_utteranceStartMs = 0;
    m_lastUtteranceEndMs = -1;
    m_samplesSeen = 0;
    m_committed.clear();
    m_finalRequests.clear();
    m_partialRequestId = 0;
    m_lastPartialMs = 0;
    m_livePartial.clear();
    emit partialTextChanged(QString());
}

void TranscriptionSession::abort()
{
    m_phase = Phase::Inactive;
    m_finalRequests.clear();
    m_partialRequestId = 0;
    m_committed.clear();
    m_livePartial.clear();
}

void TranscriptionSession::feed(const QVector<float> &chunk)
{
    if (m_phase != Phase::Recording)
        return;

    // Slice the incoming audio into fixed 30 ms analysis windows.
    m_windowBuf += chunk;
    int offset = 0;
    while (m_windowBuf.size() - offset >= kWindow) {
        processWindow(m_windowBuf.constData() + offset, kWindow);
        offset += kWindow;
    }
    m_windowBuf.remove(0, offset);
}

void TranscriptionSession::processWindow(const float *data, int count)
{
    m_samplesSeen += count;
    const qint64 now = nowMs();

    double sq = 0.0;
    for (int i = 0; i < count; ++i)
        sq += double(data[i]) * double(data[i]);
    const double rms = std::sqrt(sq / count);

    const bool voice = m_gate.update(rms, now);

    if (!m_inUtterance) {
        // Keep a short pre-roll so the first word isn't clipped.
        std::copy(data, data + count, std::back_inserter(m_preroll));
        const int maxPreroll = 16000 * 3 / 10; // 300 ms
        if (m_preroll.size() > maxPreroll)
            m_preroll.remove(0, m_preroll.size() - maxPreroll);

        if (voice) {
            m_inUtterance = true;
            m_utterance = m_preroll;
            m_preroll.clear();
            m_utteranceStartMs = now - qint64(m_utterance.size() / 16);
            m_lastPartialMs = now;
        }
        return;
    }

    std::copy(data, data + count, std::back_inserter(m_utterance));

    const qint64 sinceVoice = now - m_gate.lastVoiceMs();
    const bool tooLong = m_utterance.size() >= 16000 * m_settings->maxUtteranceSec();
    if (sinceVoice >= m_settings->silenceMs())
        finalizeUtterance(false);
    else if (tooLong)
        finalizeUtterance(true);
    else
        maybeRequestPartial();
}

void TranscriptionSession::maybeRequestPartial()
{
    const qint64 now = nowMs();
    if (m_partialRequestId != 0 || now - m_lastPartialMs < m_settings->partialIntervalMs())
        return;
    if (m_utterance.size() < 16000 * 4 / 5) // wait for at least 0.8 s of audio
        return;

    m_partialRequestId = m_nextRequestId++;
    m_lastPartialMs = now;
    emit requestTranscribe(m_partialRequestId, m_utterance, m_settings->language(),
                           promptContext(), false);
}

void TranscriptionSession::finalizeUtterance(bool force)
{
    if (!m_inUtterance)
        return;
    m_inUtterance = false;

    const qint64 now = nowMs();
    const qint64 voicedMs = m_gate.lastVoiceMs() - m_utteranceStartMs;
    QVector<float> audio = std::move(m_utterance);
    m_utterance = QVector<float>();

    // Trim trailing silence (keep ~250 ms after the last voiced window).
    if (!force) {
        const qint64 keepMs = (m_gate.lastVoiceMs() - m_utteranceStartMs) + 250;
        const int keepSamples = int(keepMs * 16); // ms -> samples at 16 kHz
        if (keepSamples > 0 && keepSamples < audio.size())
            audio.resize(keepSamples);
    }

    const qint64 pauseBefore = m_lastUtteranceEndMs < 0 ? 0 : m_utteranceStartMs - m_lastUtteranceEndMs;
    m_lastUtteranceEndMs = force ? now : m_gate.lastVoiceMs();
    m_gate.reset();

    if (voicedMs < m_settings->minUtteranceMs() && !force) {
        m_livePartial.clear();
        emitPartial();
        return; // too short — treat as noise
    }

    Utterance u;
    u.pauseBeforeMs = pauseBefore;
    m_committed.append(u);
    const quint64 id = m_nextRequestId++;
    m_finalRequests.insert(id, m_committed.size() - 1);
    m_livePartial.clear();
    emit requestTranscribe(id, audio, m_settings->language(), promptContext(), true);
}

void TranscriptionSession::end()
{
    if (m_phase != Phase::Recording)
        return;
    if (m_inUtterance)
        finalizeUtterance(true);
    m_phase = Phase::Draining;
    assembleIfDone();
}

void TranscriptionSession::onTranscribed(quint64 id, const QString &text)
{
    if (id == m_partialRequestId) {
        m_partialRequestId = 0;
        if (m_inUtterance || m_phase == Phase::Recording) {
            m_livePartial = text;
            emitPartial();
        }
        return;
    }

    const auto it = m_finalRequests.constFind(id);
    if (it == m_finalRequests.constEnd())
        return; // stale request from an aborted session
    m_committed[it.value()].text = text;
    m_committed[it.value()].decoded = true;
    m_finalRequests.erase(it);
    emitPartial();
    assembleIfDone();
}

void TranscriptionSession::assembleIfDone()
{
    if (m_phase != Phase::Draining || !m_finalRequests.isEmpty())
        return;

    QList<TextFormatter::Utterance> parts;
    parts.reserve(m_committed.size());
    for (const Utterance &u : m_committed)
        parts.append({u.text, u.pauseBeforeMs});

    m_phase = Phase::Inactive;
    emit finished(TextFormatter::format(parts, formatOptions()));
}

void TranscriptionSession::emitPartial()
{
    QList<TextFormatter::Utterance> parts;
    parts.reserve(m_committed.size() + 1);
    for (const Utterance &u : m_committed)
        parts.append({u.text, u.pauseBeforeMs});
    if (!m_livePartial.isEmpty())
        parts.append({m_livePartial, 0});
    emit partialTextChanged(TextFormatter::format(parts, formatOptions()));
}

QString TranscriptionSession::promptContext() const
{
    // Feed the tail of what was already said back to whisper so casing,
    // punctuation and vocabulary stay consistent across utterances.
    QString ctx;
    for (auto it = m_committed.crbegin(); it != m_committed.crend() && ctx.size() < 200; ++it) {
        if (it->decoded)
            ctx.prepend(it->text + u' ');
    }
    return ctx.trimmed();
}
