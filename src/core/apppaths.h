#pragma once

// ─────────────────────────────────────────────────────────────────────────────
//  Moving a user's settings and data when the organisation name changes.
//
//  QSettings and QStandardPaths both key off QCoreApplication's organisation
//  and application names, so renaming either silently relocates everything a
//  person has: their preferences, their session token, their cached data — and
//  for Sotto, gigabytes of downloaded speech models. Nothing breaks loudly. The
//  app just starts up looking freshly installed.
//
//  So a rename is only safe if something moves the old location to the new one
//  first. That is this file.
//
//  ── What it does not do ─────────────────────────────────────────────────────
//
//  It moves *this app's* settings file and *this app's* data directory, and
//  nothing else. In particular it leaves the org-level `Handoff/` directory
//  alone: that one is shared between Compose, Snap and the hub, so two apps
//  starting at once would race to move it, and its whole contents are an inbox
//  of in-flight transfers that the receiving app recreates on demand. Dropping
//  a handoff that was mid-flight during an upgrade is a smaller problem than
//  two processes renaming the same directory.
// ─────────────────────────────────────────────────────────────────────────────

#include <QString>

namespace apppaths {

/**
 * Move this app's settings and data out of a previous organisation name.
 *
 * Call it *after* setOrganizationName() and setApplicationName() — it resolves
 * the destination through QStandardPaths, which reads them — and *before*
 * anything constructs a QSettings or touches AppLocalDataLocation.
 *
 * `previousApplication` defaults to the current application name, which is the
 * usual case: only the organisation changed. Sotto passes both, because it used
 * to be organisation "sotto", application "sotto".
 *
 * Safe to call on every launch. It does nothing once the new location has
 * content, so it cannot overwrite newer state with older, and it leaves the old
 * location in place rather than deleting it — a migration that goes wrong
 * should be recoverable by hand.
 */
void migrateOrganisation(const QString &previousOrganisation,
                         const QString &previousApplication = QString());

}  // namespace apppaths
