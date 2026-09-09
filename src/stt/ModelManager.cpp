#include "stt/ModelManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QVariantMap>

namespace {
QString fileNameFor(const QString &id)
{
    return QStringLiteral("ggml-%1.bin").arg(id);
}

QString urlFor(const QString &id)
{
    return QStringLiteral("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/%1")
        .arg(fileNameFor(id));
}
} // namespace

const QList<ModelManager::ModelInfo> &ModelManager::catalog()
{
    static const QList<ModelInfo> k = {
        {QStringLiteral("tiny"), QStringLiteral("Tiny"), 78, false, false},
        {QStringLiteral("tiny.en"), QStringLiteral("Tiny (English-only)"), 78, true, false},
        {QStringLiteral("base"), QStringLiteral("Base"), 148, false, false},
        {QStringLiteral("base.en"), QStringLiteral("Base (English-only)"), 148, true, false},
        {QStringLiteral("small"), QStringLiteral("Small"), 488, false, false},
        {QStringLiteral("small.en"), QStringLiteral("Small (English-only)"), 488, true, false},
        {QStringLiteral("medium"), QStringLiteral("Medium"), 1530, false, false},
        {QStringLiteral("medium.en"), QStringLiteral("Medium (English-only)"), 1530, true, false},
        {QStringLiteral("large-v3-turbo"), QStringLiteral("Large v3 Turbo"), 1620, false, true},
        {QStringLiteral("large-v3"), QStringLiteral("Large v3"), 3100, false, false},
    };
    return k;
}

ModelManager::ModelManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

QString ModelManager::modelsDir()
{
    // AppLocalDataLocation, not AppDataLocation. Identical on Linux; on Windows
    // AppDataLocation is the *roaming* profile, which is the wrong home for
    // multi-gigabyte model blobs (a domain login would try to sync them) and,
    // worse, is not where apppaths::migrateOrganisation() looks — that builds
    // its paths from GenericDataLocation, which is %LOCALAPPDATA%. Models
    // downloaded under the old organisation name were therefore left behind on
    // Windows and the app reported them uninstalled.
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/models");
}

QString ModelManager::pathFor(const QString &id) const
{
    return modelsDir() + u'/' + fileNameFor(id);
}

bool ModelManager::isInstalled(const QString &id) const
{
    return QFile::exists(pathFor(id));
}

QVariantList ModelManager::models() const
{
    QVariantList out;
    for (const ModelInfo &m : catalog()) {
        QVariantMap e;
        e[QStringLiteral("id")] = m.id;
        e[QStringLiteral("label")] = m.label;
        e[QStringLiteral("sizeMB")] = m.sizeMB;
        e[QStringLiteral("englishOnly")] = m.englishOnly;
        e[QStringLiteral("recommended")] = m.recommended;
        e[QStringLiteral("installed")] = isInstalled(m.id);
        e[QStringLiteral("downloading")] = (m.id == m_downloadingId);
        out.append(e);
    }
    return out;
}

void ModelManager::download(const QString &id)
{
    if (!m_downloadingId.isEmpty() || isInstalled(id))
        return;

    QDir().mkpath(modelsDir());
    m_file = new QFile(pathFor(id) + QStringLiteral(".part"), this);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete m_file;
        m_file = nullptr;
        emit downloadFinished(id, false, tr("Cannot write to %1").arg(modelsDir()));
        return;
    }

    m_downloadingId = id;
    m_progress = 0.0;
    emit downloadStateChanged();
    emit modelsChanged();

    QNetworkRequest req{QUrl(urlFor(id))};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("sotto/") + QStringLiteral(SOTTO_VERSION));
    m_reply = m_nam->get(req);

    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_file)
            m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        m_progress = total > 0 ? double(got) / double(total) : 0.0;
        emit downloadStateChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        const bool ok = m_reply->error() == QNetworkReply::NoError;
        finishDownload(ok, ok ? QString() : m_reply->errorString());
    });
}

void ModelManager::finishDownload(bool ok, const QString &error)
{
    const QString id = m_downloadingId;
    QString err = error;

    if (m_file) {
        m_file->write(m_reply ? m_reply->readAll() : QByteArray());
        m_file->close();
        const QString partPath = m_file->fileName();

        if (ok) {
            // Validate: ggml files start with the "lmgg" magic and are large.
            QFile check(partPath);
            QByteArray magic;
            if (check.open(QIODevice::ReadOnly))
                magic = check.read(4);
            check.close();
            if (magic != QByteArrayLiteral("lmgg") || QFileInfo(partPath).size() < 10'000'000) {
                ok = false;
                err = tr("Downloaded file is not a valid ggml model.");
            }
        }

        if (ok) {
            QFile::remove(pathFor(id));
            ok = QFile::rename(partPath, pathFor(id));
            if (!ok)
                err = tr("Could not move the model into place.");
        }
        if (!ok)
            QFile::remove(partPath);

        delete m_file;
        m_file = nullptr;
    }

    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_downloadingId.clear();
    m_progress = 0.0;
    emit downloadStateChanged();
    emit modelsChanged();
    emit downloadFinished(id, ok, err);
}

void ModelManager::cancelDownload()
{
    if (m_reply)
        m_reply->abort(); // finished() handler does the cleanup
}

void ModelManager::remove(const QString &id)
{
    QFile::remove(pathFor(id));
    emit modelsChanged();
}
