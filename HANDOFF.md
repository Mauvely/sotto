# Handoff — 2026-10-14

## A Flatpak again, and the bug it found (later the same day)

`packaging/linux/build-flatpak.sh` (app-base's, byte for byte) builds Sotto's
Flatpak again from `packaging/linux/flatpak/app.yml.in`, and `release.yml` has a
`flatpak` job on GitHub's `ubuntu-24.04` runner, for Mauvely OS's remote.
app-base's `HANDOFF.md` has the shared half.

**The Flatpak exited on every launch, silently, with status 0.** In the
sandbox an app may own only bus names under its own id. `registerService()`
takes `com.mauvely.sotto`, then the legacy `net.mauvely.sotto` as a lock; the
sandbox refused the second, `registerService()` returned false, and Sotto read
that as another instance running, forwarded to it and quit. The manifest now
has `--own-name=net.mauvely.sotto`. **Remove that line with the legacy names**
(the list below says so). After the fix the bundle installed from a
throwaway-key repo on `stable` and opened its settings window in `hsway` as
`com.mauvely.sotto`. Neither the hotkey nor dictation ran there: no portal and
no microphone.

**Releasing it needs a version bump.** The feed has a `flatpak` row for 0.1.0,
the old-id bundle from before 2026-09-12, so the gate answers
`need_flatpak=false` for 0.1.0. 0.1.1 builds every lane that has no row.

## Sotto's id is `com.mauvely.sotto` (plan 20, step 3)

Plan 20 is `website-internal/docs/launch/plans/20-app-ids-to-com.md`, on that
repo's `staging` branch. Timur decided the form on 2026-10-14:
`com.mauvely.<key>`, no `.app`. Sotto was `net.mauvely.sotto.app`.
`cmake/MauvelyAppInfo.cmake` is app-base's, byte for byte
(`cc16fc4d62a3e01b0623ee709f1a03158e2b9929`), and its guard fails configure
unless `APP_ID` is exactly `com.mauvely.sotto`.

Sotto used to type its id in five places. Now it is stated once, in
`mauvely_app_info()`, and read as `APP_ID` in `main.cpp`
(`setDesktopFileName`), `App.cpp` (the portal host app), `Notifier.cpp` (the
notification `app_icon`) and `Settings.cpp` (the autostart file name and its
`Icon=`). The generated `.desktop`, its `Icon=` and `StartupWMClass` and the
metainfo `<id>` already derived from it. `SingleInstance.cpp` (Windows only)
names its pipe and mutex from it too.

There is no `MACOSX_BUNDLE_GUI_IDENTIFIER` here, and no packaging file named by
the id (all are generated; the Flatpak lane is gone).

## What migrates, and what does not

- **Autostart, Linux.** `Settings::migrateAutostart(APP_PREVIOUS_ID)` runs in
  `main()` after the instance lock is won (not in the screenshot or download
  harnesses, not on a forwarding second launch). It moves
  `~/.config/autostart/net.mauvely.sotto.app.desktop` to
  `com.mauvely.sotto.desktop`, rewrites only a key whose whole value is the old
  id (`Icon=`), keeps `Exec`. With both present the old one is removed.
  `tests/test_autostart_migration.cpp` pins it; it compiles `Settings.cpp`
  into the test, which gets `APP_ID` and `APP_PREVIOUS_ID` through
  `EXTRA_TARGETS` in the `mauvely_app_info()` call. Keep the migration for as
  long as builds from before 2026-10-14 may be installed; it is harmless.
- **Windows.** The Run value is named "Sotto", not by the id, so nothing moves
  there. The pipe and mutex names moved; an old and a new instance miss each
  other once, during the upgrade.
- **Settings and data** are keyed by organisation and display name, not the id.
  Nothing to move.
- **The portals key by app id**, so the GlobalShortcuts binding and the
  RemoteDesktop permission start afresh under the new id. A stale
  `portal/remoteDesktopRestoreToken` stays in `Sotto.conf`; the portal should
  ignore a token it does not know and ask again. None of this was run: the
  checks used a private bus with no portal.

## Remove next release: the old D-Bus names

People script against `net.mauvely.sotto` / `net.mauvely.Sotto`
(`website-docs/…/notepad-and-cli.mdx`). The new names are `com.mauvely.sotto`
and `com.mauvely.Sotto`, written out in `DBusService.h` rather than derived
from `APP_ID`, because they are API. For one release the old ones also work.
To remove them, in `src/core/DBusService.{h,cpp}` and the manifest:

- `kLegacyService`, `kLegacyInterface`;
- `LegacyDBusAdaptor` (class, constructor, and the `new LegacyDBusAdaptor(this)`
  in the `DBusService` constructor); `ExportAdaptors` can go with it;
- the second `registerService(kLegacyService)` in `registerService()`;
- the fallback to the old names in `callRunningInstance()`.
- the `--own-name=net.mauvely.sotto` line in
  `packaging/linux/flatpak/app.yml.in`.

The last two are not only for scripts. The old name is a lock too: if an older
Sotto is still running when the new one starts, the new one gives its name back
and forwards to the old one, instead of two copies fighting over the tray and
the hotkey. Then edit the docs page and the README paragraph together with it,
and `docs/ARCHITECTURE.md` needs no change.

## Unverified

- **Windows.** Nothing was built there. `SingleInstance.cpp`'s `SOTTO_WIDE`
  macro (it makes `L"…"` out of `APP_ID`) was only compiled in isolation with
  g++ and printed `Local\com.mauvely.sotto.instance`. `Settings::migrateAutostart`
  is an empty function on Windows. CI's Windows leg is its first real compile.
- **The AppImage** was not built.
- **D-Bus has no test.** `DBusService` takes the whole `App`, which is the
  seam that is missing. It was checked by hand, in `hsway` on a private bus.

## How the by-hand check ran

A private `dbus-daemon` with a config that has no `<servicedir>`, so nothing
activates and at-spi never takes over the real session's socket, started with
`DISPLAY` unset. Sotto ran under `hsway` with `DBUS_SESSION_BUS_ADDRESS`,
`XDG_CONFIG_HOME` and `XDG_DATA_HOME` pointed at scratch directories. `swaymsg`
showed `app_id` `com.mauvely.sotto`. `busctl --user` introspection showed both
interfaces on `/Sotto`, both bus names owned by the one process, and a
`ShowNotepad` through each name opened the notepad. A stand-in that owned only
`net.mauvely.sotto` received a forwarded `--notepad` from the new binary, which
did not become a second instance.

## Still open from earlier sessions

`git show cacf8da:HANDOFF.md` has them: the Windows overlay's box shadow was
seen once in a capture and not with a pointer, and the startup `qInfo()` line
does not reach a pipe from the Windows GUI binary.
