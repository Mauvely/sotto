#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QTimer;

// Whisper ggml model catalog + downloader. Models are fetched from the
// canonical whisper.cpp collection on Hugging Face into the app's local data
// directory (see modelsDir()) and validated by ggml magic bytes.
//
// Everything about the transfer that can fail now says so. It used to be
// possible for a download to sit at 0% forever: `downloadFinished` had no
// listener anywhere in the QML, `QFile::write`'s return value was dropped, and
// nothing noticed a reply that connected and then delivered nothing — which is
// exactly what a 3 GB body did here. See download().
class ModelManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList models READ models NOTIFY modelsChanged)
    Q_PROPERTY(QString downloadingId READ downloadingId NOTIFY downloadStateChanged)
    Q_PROPERTY(double downloadProgress READ downloadProgress NOTIFY downloadStateChanged)
    // "34.2 / 78 MB · 12 MB/s", or "connecting…" before the first byte.
    Q_PROPERTY(QString downloadStatus READ downloadStatus NOTIFY downloadStateChanged)

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
    QString downloadStatus() const { return m_status; }

    Q_INVOKABLE void download(const QString &id);
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void remove(const QString &id);
    Q_INVOKABLE bool isInstalled(const QString &id) const;

    QString pathFor(const QString &id) const;
    static QString modelsDir();
    static const QList<ModelInfo> &catalog();
    static int catalogSizeMB(const QString &id);

signals:
    void modelsChanged();
    void downloadStateChanged();
    void downloadFinished(const QString &id, bool ok, const QString &error);

private:
    void startTransfer();
    void retryOrFail(const QString &why);
    void finishDownload(bool ok, const QString &error);
    void onReadyRead();
    void onStallCheck();
    void setStatus(const QString &s);

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    QTimer *m_stallTimer = nullptr;
    QString m_downloadingId;
    QString m_status;
    double m_progress = 0.0;
    int m_attempt = 0;      // 0 = first try; retries resume with a Range request
    bool m_cancelled = false;
    qint64 m_received = 0;  // total bytes on disk, including a resumed prefix
    qint64 m_baseOffset = 0; // what was already there when this attempt started
    qint64 m_expected = 0;
    qint64 m_lastSeen = 0;  // bytes at the previous stall check
    QElapsedTimer m_clock;  // time since progress last moved
};
