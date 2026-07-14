#include "audio/AudioCapture.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>
#include <QtMath>

#include <utility>

namespace {
QAudioDevice findDevice(const QString &name)
{
    if (name.isEmpty() || name == QStringLiteral("default"))
        return QMediaDevices::defaultAudioInput();
    const auto inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &d : inputs) {
        if (d.description() == name)
            return d;
    }
    return QMediaDevices::defaultAudioInput();
}
} // namespace

AudioCapture::AudioCapture(QObject *parent)
    : QObject(parent)
{
}

AudioCapture::~AudioCapture()
{
    stop();
}

QStringList AudioCapture::availableDevices()
{
    QStringList out{QStringLiteral("default")};
    const auto inputs = QMediaDevices::audioInputs();
    for (const QAudioDevice &d : inputs)
        out << d.description();
    out.removeDuplicates();
    return out;
}

bool AudioCapture::start(const QString &deviceName)
{
    stop();

    const QAudioDevice device = findDevice(deviceName);
    if (device.isNull()) {
        emit errorOccurred(tr("No audio input device found."));
        return false;
    }

    QAudioFormat wanted;
    wanted.setSampleRate(kTargetRate);
    wanted.setChannelCount(1);
    wanted.setSampleFormat(QAudioFormat::Float);
    m_fmt = device.isFormatSupported(wanted) ? wanted : device.preferredFormat();
    if (m_fmt.sampleRate() <= 0 || m_fmt.channelCount() <= 0) {
        emit errorOccurred(tr("Audio device \"%1\" reports no usable format.").arg(device.description()));
        return false;
    }

    m_step = double(m_fmt.sampleRate()) / double(kTargetRate);
    m_pos = 0.0;
    m_carry.clear();
    m_sqSum = 0.0;
    m_sqCount = 0;

    m_source = new QAudioSource(device, m_fmt, this);
    m_io = m_source->start();
    if (!m_io) {
        emit errorOccurred(tr("Could not open audio input \"%1\".").arg(device.description()));
        stop();
        return false;
    }
    connect(m_io, &QIODevice::readyRead, this, &AudioCapture::onReadyRead);
    return true;
}

void AudioCapture::stop()
{
    if (m_io) {
        disconnect(m_io, nullptr, this, nullptr);
        m_io = nullptr;
    }
    if (m_source) {
        m_source->stop();
        m_source->deleteLater();
        m_source = nullptr;
    }
}

void AudioCapture::onReadyRead()
{
    if (!m_io)
        return;
    const QByteArray bytes = m_io->readAll();
    if (bytes.isEmpty())
        return;

    const QVector<float> out = resample(toMonoFloat(bytes));
    if (out.isEmpty())
        return;

    // Level metering: emit roughly every 33 ms of audio.
    for (float s : out) {
        m_sqSum += double(s) * double(s);
        if (++m_sqCount >= kTargetRate / 30) {
            const double rms = std::sqrt(m_sqSum / m_sqCount);
            const double db = 20.0 * std::log10(rms + 1e-9);
            emit level(float(qBound(0.0, (db + 50.0) / 40.0, 1.0)));
            m_sqSum = 0.0;
            m_sqCount = 0;
        }
    }

    emit samples(out);
}

QVector<float> AudioCapture::toMonoFloat(const QByteArray &bytes) const
{
    const int channels = m_fmt.channelCount();
    const int frameBytes = m_fmt.bytesPerFrame();
    if (frameBytes <= 0)
        return {};
    const int frames = int(bytes.size()) / frameBytes;

    QVector<float> mono(frames);
    const char *p = bytes.constData();

    for (int f = 0; f < frames; ++f) {
        double acc = 0.0;
        for (int c = 0; c < channels; ++c) {
            const char *sp = p + f * frameBytes + c * m_fmt.bytesPerSample();
            switch (m_fmt.sampleFormat()) {
            case QAudioFormat::Float:
                acc += *reinterpret_cast<const float *>(sp);
                break;
            case QAudioFormat::Int16:
                acc += *reinterpret_cast<const qint16 *>(sp) / 32768.0;
                break;
            case QAudioFormat::Int32:
                acc += *reinterpret_cast<const qint32 *>(sp) / 2147483648.0;
                break;
            case QAudioFormat::UInt8:
                acc += (*reinterpret_cast<const quint8 *>(sp) - 128) / 128.0;
                break;
            default:
                break;
            }
        }
        mono[f] = float(acc / channels);
    }
    return mono;
}

QVector<float> AudioCapture::resample(QVector<float> mono)
{
    if (m_fmt.sampleRate() == kTargetRate) {
        if (m_carry.isEmpty())
            return mono;
        m_carry += mono;
        return std::exchange(m_carry, {});
    }

    m_carry += mono;
    QVector<float> out;
    out.reserve(int(m_carry.size() / m_step) + 1);

    while (m_pos + 1.0 < m_carry.size()) {
        const int i = int(m_pos);
        const float frac = float(m_pos - i);
        out.append(m_carry[i] * (1.0f - frac) + m_carry[i + 1] * frac);
        m_pos += m_step;
    }

    // Drop consumed source samples, keeping one for cross-chunk interpolation.
    const int consumed = qMax(0, int(m_pos) - 1);
    m_carry.remove(0, consumed);
    m_pos -= consumed;
    return out;
}
