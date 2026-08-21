#include "core/apppaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>
#include <QtGlobal>

namespace apppaths {
namespace {

/**
 * Where a given (organisation, application) pair keeps its data.
 *
 * Derived from GenericDataLocation rather than read from AppLocalDataLocation,
 * because QStandardPaths answers that one for the *current* names and there is
 * no way to ask it about a previous pair. On Linux and Windows the layout is
 * `<generic>/<org>/<app>`, which is the whole of what this reconstructs.
 */
QString dataDirFor(const QString &organisation, const QString &application) {
    const QString generic =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (generic.isEmpty() || organisation.isEmpty() || application.isEmpty()) return {};
    return generic + QLatin1Char('/') + organisation + QLatin1Char('/') + application;
}

/** True when `dir` exists and holds something worth keeping. */
bool hasContent(const QString &dir) {
    if (dir.isEmpty()) return false;
    QDir d(dir);
    return d.exists() && !d.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
}

}  // namespace

void migrateOrganisation(const QString &previousOrganisation,
                         const QString &previousApplication) {
    const QString currentOrg = QCoreApplication::organizationName();
    const QString currentApp = QCoreApplication::applicationName();
    const QString oldApp = previousApplication.isEmpty() ? currentApp : previousApplication;

    if (previousOrganisation.isEmpty() || currentOrg.isEmpty() || currentApp.isEmpty()) return;
    if (previousOrganisation == currentOrg && oldApp == currentApp) return;

    // ── Settings ────────────────────────────────────────────────────────────
    //
    // Copied key by key through QSettings rather than by moving a file, because
    // this has to work on Windows too — where the "file" is a registry subtree
    // under HKCU\Software\<org>\<app>, and there is nothing to rename.
    {
        QSettings current;
        QSettings previous(previousOrganisation, oldApp);
        const QStringList keys = previous.allKeys();
        if (current.allKeys().isEmpty() && !keys.isEmpty()) {
            for (const QString &key : keys) current.setValue(key, previous.value(key));
            current.sync();
            if (current.status() == QSettings::NoError) {
                qInfo("migrated %lld settings from \"%s/%s\"",
                      static_cast<long long>(keys.size()),
                      qUtf8Printable(previousOrganisation), qUtf8Printable(oldApp));
            } else {
                // Reported, not fatal. The app is perfectly usable with default
                // settings; refusing to start because a migration failed would
                // turn a cosmetic rename into an outage.
                qWarning("could not write migrated settings (QSettings status %d)",
                         int(current.status()));
            }
        }
    }

    // ── Data ────────────────────────────────────────────────────────────────
    //
    // Renamed, not copied: Sotto's directory holds multi-gigabyte speech models
    // and a copy would need the space twice and take minutes at startup. A
    // rename within one filesystem is atomic and instant, and both paths live
    // under the same data root, so there is no cross-device case to handle.
    const QString from = dataDirFor(previousOrganisation, oldApp);
    const QString to = dataDirFor(currentOrg, currentApp);
    if (from.isEmpty() || to.isEmpty() || from == to) return;
    if (!hasContent(from) || hasContent(to)) return;

    // An empty directory Qt created at the new path on an earlier launch would
    // block the rename on most platforms, so clear it out of the way first.
    QDir().rmdir(to);
    QDir().mkpath(QFileInfo(to).absolutePath());

    if (QDir().rename(from, to)) {
        qInfo("migrated data directory to \"%s\"", qUtf8Printable(to));
    } else {
        qWarning("could not move \"%s\" to \"%s\" — the old data is still there",
                 qUtf8Printable(from), qUtf8Printable(to));
    }
}

}  // namespace apppaths
