#pragma once

#include <QAudioFormat>
#include <QObject>
#include <QStringList>
#include <QVector>

class QAudioSource;
class QIODevice;

// Microphone capture. Whatever format the device delivers is converted to
// the 16 kHz mono float stream whisper.cpp expects (channel averaging +
// linear resampling). Also emits a smoothed input level for the visualiser.
class AudioCapture : public QObject
{
    Q_OBJECT
public:
    static constexpr int kTargetRate = 16000;

    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture() override;

    // deviceName: "default" or a device description as returned by availableDevices()
    bool start(const QString &deviceName);
    void stop();
    bool active() const { return m_source != nullptr; }

    static QStringList availableDevices();

signals:
    void samples(const QVector<float> &chunk); // 16 kHz mono float
    void level(float normalized);              // 0..1, ~30 Hz
    void errorOccurred(const QString &message);

private:
    void onReadyRead();
    QVector<float> toMonoFloat(const QByteArray &bytes) const;
    QVector<float> resample(QVector<float> mono);

    QAudioSource *m_source = nullptr;
    QIODevice *m_io = nullptr;
    QAudioFormat m_fmt;

    // resampler state
    double m_step = 1.0;      // source samples per output sample
    double m_pos = 0.0;       // fractional read position into m_carry
    QVector<float> m_carry;   // unconsumed source samples across chunks

    // level metering
    double m_sqSum = 0.0;
    int m_sqCount = 0;
};
