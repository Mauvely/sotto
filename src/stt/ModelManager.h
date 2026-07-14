#pragma once

#include <QObject>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;

// Whisper ggml model catalog + downloader. Models are fetched from the
// canonical whisper.cpp collection on Hugging Face into
// ~/.local/share/sotto/models and validated by ggml magic bytes.
class ModelManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QString downloadingId READ downloadingId NOTIFY downloadStateChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadStateChanged)

public:
    struct ModelInfo {
        QString id;      // e.g. "large-v3-turbo" -> ggml-large-v3-turbo.bin
        QString label;
        int sizeMB;
        bool englishOnly;
        bool recommended;
    };

    explicit ModelManager(QObject *parent = nullptr);

    QVariantList models() const;
    QString downloadingId() const { return m_downloadingId; }
    double downloadProgress() const { return m_progress; }

    Q_INVOKABLE void download(const QString &id);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE bool isInstalled(const QString &id) const;

    QString pathFor(const QString &id) const;
    static QString modelsDir();
    static const QList<ModelInfo> &catalog();

signals:
    void modelsChanged();
    void downloadStateChanged();
    void downloadFinished(const QString &id, bool ok, const QString &error);

private:
    void finishDownload(bool ok, const QString &error);

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    QString m_downloadingId;
    double m_progress = 0.0;
};
