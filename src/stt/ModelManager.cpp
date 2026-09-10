#include "stt/ModelManager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTimer>
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

QString humanMB(qint64 bytes)
{
    if (bytes >= 1'000'000'000)
        return QStringLiteral("%1 GB").arg(double(bytes) / 1e9, 0, 'f', 2);
    return QStringLiteral("%1 MB").arg(double(bytes) / 1e6, 0, 'f', 1);
}

// No progress at all for this long is not a slow connection, it is a dead
// transfer. Generous, because the first byte of a Hugging Face LFS object comes
// after a redirect to a CDN that may itself have to fetch from origin.
constexpr int kStallSeconds = 45;
constexpr int kStallTickMs = 1000;
constexpr int kMaxAttempts = 3;
constexpr int kRetryDelayMs = 2000;
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

int ModelManager::catalogSizeMB(const QString &id)
{
    for (const ModelInfo &m : catalog())
        if (m.id == id)
            return m.sizeMB;
    return 0;
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

void ModelManager::setStatus(const QString &s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit downloadStateChanged();
}

void ModelManager::download(const QString &id)
{
    if (!m_downloadingId.isEmpty() || isInstalled(id))
        return;

    const QString dir = modelsDir();
    if (!QDir().mkpath(dir)) {
        emit downloadFinished(id, false, tr("Cannot create %1").arg(QDir::toNativeSeparators(dir)));
        return;
    }

    // Refuse before spending the bandwidth rather than after. A model that only
    // fails its size check at the end looks like a corrupt download.
    const qint64 need = qint64(catalogSizeMB(id)) * 1'000'000;
    const QStorageInfo storage(dir);
    if (need > 0 && storage.isValid() && storage.bytesAvailable() > 0
        && storage.bytesAvailable() < need + 50'000'000) {
        emit downloadFinished(id, false,
                              tr("Not enough space in %1 — %2 needed, %3 free.")
                                  .arg(QDir::toNativeSeparators(dir), humanMB(need),
                                       humanMB(storage.bytesAvailable())));
        return;
    }

    m_downloadingId = id;
    m_attempt = 0;
    m_progress = 0.0;
    m_received = 0;
    m_lastSeen = 0;
    m_baseOffset = 0;
    m_expected = need; // replaced by Content-Length as soon as one arrives
    m_status = tr("connecting…");
    emit downloadStateChanged();
    emit modelsChanged();

    startTransfer();
}

// One GET, possibly a ranged one continuing a `.part` a previous attempt left
// behind. Split out from download() because a stall or a dropped connection
// re-enters here rather than failing the whole thing.
void ModelManager::startTransfer()
{
    const QString partPath = pathFor(m_downloadingId) + QStringLiteral(".part");
    // Append, not Truncate, whenever there is something worth continuing: the
    // retries below, and the "it failed, so I pressed Download again" case,
    // which is the one that matters for a three-gigabyte model. A `.part`
    // already at or past the expected size is not a resume, it is debris.
    const qint64 onDisk = QFileInfo(partPath).size();
    const bool resume = onDisk > 0 && (m_attempt > 0 || m_expected <= 0 || onDisk < m_expected);
    m_file = new QFile(partPath, this);
    const QIODevice::OpenMode mode = resume ? QIODevice::WriteOnly | QIODevice::Append
                                            : QIODevice::WriteOnly | QIODevice::Truncate;
    if (!m_file->open(mode)) {
        const QString why = m_file->errorString();
        delete m_file;
        m_file = nullptr;
        finishDownload(false,
                       tr("Cannot write to %1 (%2)")
                           .arg(QDir::toNativeSeparators(modelsDir()), why));
        return;
    }
    m_baseOffset = m_file->size();
    m_received = m_baseOffset;
    m_lastSeen = m_received;
    m_clock.start();

    QNetworkRequest req{QUrl(urlFor(m_downloadingId))};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("sotto/") + QStringLiteral(SOTTO_VERSION));
    // Hugging Face answers `resolve/main/...` with a 302 to a CDN on another
    // host, so following redirects is not optional. Set per request as well as
    // on the manager: the manager-wide policy is the one a future refactor is
    // most likely to move.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    // Bounds what Qt buffers for us between readyRead deliveries. Without it a
    // fast CDN can hold a large fraction of a multi-gigabyte body in memory if
    // the GUI thread is busy.
    req.setAttribute(QNetworkRequest::MaximumDownloadBufferSizeAttribute, 8 * 1024 * 1024);
    if (m_baseOffset > 0) {
        // The CDN advertises `accept-ranges: bytes` and answers 206, so a retry
        // continues rather than starting three gigabytes again.
        req.setRawHeader("Range", "bytes=" + QByteArray::number(m_baseOffset) + "-");
    }

    m_reply = m_nam->get(req);
    m_reply->setReadBufferSize(8 * 1024 * 1024);

    connect(m_reply, &QNetworkReply::readyRead, this, &ModelManager::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        // `total` is -1 until the CDN's headers arrive, stays -1 for a chunked
        // body, and on a 206 counts only the *remaining* bytes — hence the
        // offset on both sides. The catalog size is the fallback, so the bar
        // moves either way rather than sitting at zero.
        if (total > 0)
            m_expected = m_baseOffset + total;
        if (m_baseOffset + got > m_received)
            m_received = m_baseOffset + got;
        m_progress = m_expected > 0 ? qBound(0.0, double(m_received) / double(m_expected), 1.0) : 0.0;
        emit downloadStateChanged();
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (!m_reply)
            return;
        if (m_reply->error() == QNetworkReply::NoError) {
            finishDownload(true, QString());
            return;
        }
        if (m_reply->error() == QNetworkReply::OperationCanceledError && m_cancelled) {
            finishDownload(false, tr("Cancelled."));
            return;
        }
        retryOrFail(m_reply->errorString());
    });

    // The watchdog. This is what turns "stuck on 0%" — a reply that connects and
    // then never delivers a byte, never errors and never finishes — into
    // something that first retries and then says what happened.
    if (!m_stallTimer) {
        m_stallTimer = new QTimer(this);
        m_stallTimer->setInterval(kStallTickMs);
        connect(m_stallTimer, &QTimer::timeout, this, &ModelManager::onStallCheck);
    }
    m_stallTimer->start();
}

void ModelManager::onReadyRead()
{
    if (!m_reply || !m_file)
        return;
    const QByteArray chunk = m_reply->readAll();
    if (chunk.isEmpty())
        return;
    // The return value used to be dropped. A short write — a full disk, most
    // likely — then produced a truncated file that failed its size check at the
    // end and was reported as "not a valid ggml model", which sends the reader
    // looking in the wrong place entirely.
    if (m_file->write(chunk) != chunk.size()) {
        const QString why = m_file->errorString();
        // Disconnect before aborting. abort() emits finished() synchronously,
        // and that handler would unwind the whole download first — so the user
        // would be told the transfer was cancelled rather than that the disk
        // refused the write, which is the only fact that matters here.
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
        m_cancelled = true; // a disk failure is not worth retrying or resuming
        finishDownload(false, tr("Writing the model to disk failed: %1").arg(why));
    }
}

void ModelManager::onStallCheck()
{
    if (!m_reply)
        return;
    if (m_received != m_lastSeen) {
        m_lastSeen = m_received;
        m_clock.restart();
    } else if (m_clock.elapsed() > kStallSeconds * 1000) {
        qWarning() << "[models] transfer of" << m_downloadingId << "stalled at" << m_received
                   << "bytes on attempt" << (m_attempt + 1);
        retryOrFail(m_received == m_baseOffset
                        ? tr("nothing arrived from huggingface.co in %1 seconds")
                              .arg(kStallSeconds)
                        : tr("the transfer went quiet for %1 seconds").arg(kStallSeconds));
        return;
    }

    if (m_received <= 0) {
        setStatus(tr("connecting…"));
        return;
    }
    setStatus(m_expected > 0 ? QStringLiteral("%1 / %2").arg(humanMB(m_received), humanMB(m_expected))
                             : humanMB(m_received));
}

// A multi-gigabyte transfer over a home connection will sometimes die, and the
// old code's answer was to delete the part file and show nothing at all. This
// keeps what arrived and picks it up again; only when the retries are used up
// does the user see a failure, and then it says what actually happened.
void ModelManager::retryOrFail(const QString &why)
{
    if (m_stallTimer)
        m_stallTimer->stop();
    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->abort();
        reply->deleteLater();
    }
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    if (m_attempt + 1 >= kMaxAttempts) {
        finishDownload(false, tr("Download failed after %1 attempts — %2.")
                                  .arg(kMaxAttempts)
                                  .arg(why));
        return;
    }

    ++m_attempt;
    setStatus(m_received > 0
                  ? tr("retrying from %1…").arg(humanMB(m_received))
                  : tr("retrying…"));
    qWarning().noquote() << "[models] retry" << m_attempt << "for" << m_downloadingId << ":" << why;
    QTimer::singleShot(kRetryDelayMs, this, [this] {
        if (!m_downloadingId.isEmpty())
            startTransfer();
    });
}

void ModelManager::finishDownload(bool ok, const QString &error)
{
    if (m_downloadingId.isEmpty())
        return; // already unwound (abort() re-entering through finished())

    const QString id = m_downloadingId;
    QString err = error;

    if (m_stallTimer) {
        m_stallTimer->stop();
        m_stallTimer->deleteLater();
        m_stallTimer = nullptr;
    }

    const QString partPath = pathFor(id) + QStringLiteral(".part");
    if (m_file) {
        if (m_reply && m_reply->isOpen())
            m_file->write(m_reply->readAll());
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }

    // `discard` is narrower than `!ok` on purpose. A network failure or a
    // transfer that ended early leaves a `.part` that is *worth* keeping —
    // pressing Download again resumes from it rather than starting three
    // gigabytes over. Only something that cannot be continued is thrown away.
    bool discard = false;
    if (ok) {
        // Validate: ggml files start with the "lmgg" magic and are large.
        QFile check(partPath);
        QByteArray magic;
        if (check.open(QIODevice::ReadOnly))
            magic = check.read(4);
        check.close();
        const qint64 got = QFileInfo(partPath).size();
        if (magic != QByteArrayLiteral("lmgg")) {
            ok = false;
            discard = true;
            err = tr("What arrived is not a ggml model (%1).").arg(humanMB(got));
        } else if (m_expected > 0 && got < m_expected) {
            // Distinct from "not a model": the file is the right kind and the
            // wrong length, which means the transfer ended early.
            ok = false;
            err = tr("The download ended early — %1 of %2. Press Download again to "
                     "carry on from there.")
                      .arg(humanMB(got), humanMB(m_expected));
        } else if (got < 10'000'000) {
            ok = false;
            discard = true;
            err = tr("The downloaded model is too small to be real (%1).").arg(humanMB(got));
        }
    }

    if (ok) {
        QFile::remove(pathFor(id));
        ok = QFile::rename(partPath, pathFor(id));
        if (!ok) {
            discard = true;
            err = tr("Could not move the model into place.");
        }
    }
    if (m_cancelled || discard)
        QFile::remove(partPath);

    if (m_reply) {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        reply->disconnect(this);
        reply->deleteLater();
    }
    m_downloadingId.clear();
    m_cancelled = false;
    m_attempt = 0;
    m_progress = 0.0;
    m_received = 0;
    m_baseOffset = 0;
    m_expected = 0;
    m_status.clear();
    emit downloadStateChanged();
    emit modelsChanged();
    if (!ok)
        qWarning().noquote() << "[models]" << id << "failed:" << err;
    emit downloadFinished(id, ok, err);
}

void ModelManager::cancelDownload()
{
    // The flag is what tells the finished() handler this abort was asked for,
    // rather than the connection dying — one retries, the other must not.
    m_cancelled = true;
    if (m_reply)
        m_reply->abort(); // finished() handler does the cleanup
    else if (!m_downloadingId.isEmpty())
        finishDownload(false, tr("Cancelled.")); // aborted between retries
}

void ModelManager::remove(const QString &id)
{
    QFile::remove(pathFor(id));
    emit modelsChanged();
}
