#pragma once

#include <QObject>
#include <QString>
#include <QVector>

struct whisper_context;

// Thin wrapper around whisper.cpp. Lives on a dedicated worker thread
// (queued slot invocations serialize the work), so a decode never blocks
// the UI. One request = one whisper_full() run over the given audio.
class WhisperEngine : public QObject
{
    Q_OBJECT
public:
    explicit WhisperEngine(QObject *parent = nullptr);
    ~WhisperEngine() override;

public slots:
    void loadModel(const QString &path);
    void unload();
    // finalPass=true enables temperature fallback for better quality on the
    // committed decode; partials use the fastest settings.
    void transcribe(quint64 id, const QVector<float> &audio, const QString &language,
                    const QString &prompt, bool finalPass);

signals:
    void modelLoaded(bool ok, const QString &systemInfo);
    void transcribed(quint64 id, const QString &text);

private:
    whisper_context *m_ctx = nullptr;
    QString m_path;
};
