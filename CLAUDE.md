# CLAUDE.md

Guidance for working in this repository. `README.md` is what a user reads and
`docs/ARCHITECTURE.md` is how the pieces fit; this file is what saves
re-discovering the same things every session.

## What this is

**Sotto** — a **Qt6/C++17 + QML** desktop app for fully local voice dictation.
A global shortcut opens a pill on the active monitor, whisper.cpp transcribes
on this machine, and the formatted text is injected into whatever has focus.
Version lives in `CMakeLists.txt` (`project(... VERSION x.y.z)`) and compiles in
as `SOTTO_VERSION`.

**No account, no network, no telemetry.** The only request Sotto ever makes is
downloading a speech model when somebody asks it to. That is the product, not a
feature of it — anything that adds a call home is a different application.

## Where it differs from every other app in the suite

Sotto was written outside the shared foundation and has only partly been brought
into it. Assumptions that hold in Compose, Snap, Relay, Mail, Play and Suite do
**not** hold here:

| | |
|---|---|
| **QML, not widgets** | The UI is `qml/` driven from `src/ui/`. There is no `FramelessWindow`, no `TitleBar`, no `ui::widgets.h`. The suite's window-chrome work — including the Windows native-frame takeover — does not apply and was correctly skipped. |
| **No `brand.h`, no `branding.{h,cpp}`** | The shared colour table and the organisation-branding port are absent, and so is `branding_interop`. Colour lives in QML: `qml/Brand.qml` is the raw ramps and `qml/Theme.qml` is the semantic layer, the two of them mirroring the siblings' `src/core/{brand,theme}.h` by hand. Nothing checks that they agree. |
| **No `core_smoke`** | Two focused test binaries instead — `test_formatter` and `test_speechgate`, both registered with CTest. |
| **`cmake/MauvelyAppInfo.cmake` is current** | It is the one shared file this repo does carry, and it is in step with app-base's. |
| **No accounts** | No `AccountService`, no `suitelink`, no entry in `KNOWN_CLIENTS`. `device-mint.test.ts` in website-main explicitly asserts `assertCanMint('suite', 'sotto')` throws. Sotto gets an account when it has something to authenticate *for*. |

## Build / run / test

```bash
cmake -S . -B build -DSOTTO_GPU=cpu     # Qt 6.4+; whisper.cpp is fetched here
cmake --build build -j8
./build/bin/MauvelySotto
ctest --test-dir build
```

`SOTTO_GPU` is `cpu | hip | cuda | vulkan`. The GPU backends need toolchains a
hosted runner does not have, and **an artifact built against one will not start
without it** — so anything shipped is `cpu`.

### Windows, verified 2026-09-06 and again 2026-09-09

It builds, and both tests pass, against Qt 6.7.3 msvc2019_64 with
`-DSOTTO_GPU=cpu`. Three things worth knowing before trying:

- **`FetchContent` of whisper.cpp needs `core.longpaths`.** whisper.cpp carries
  paths like `examples/whisper.android.java/app/src/androidTest/java/com/…`
  which pass 260 characters once a build directory is prepended, and the
  checkout fails with "Filename too long". `GIT_CONFIG core.longpaths=true` is
  on the `FetchContent_Declare` for exactly this. It is per-clone on purpose:
  the alternative is telling somebody to run `git config --system` as an
  administrator. GitHub's runners check out at `D:\a\sotto\sotto` and are short
  enough to have never hit it, which is why this surfaced on a developer's
  machine rather than in CI.
- **`--version` and `--help` print nothing through a pipe.** The target is
  `WIN32_EXECUTABLE`, so it has no stdout; `attachParentConsole()` attaches to
  the calling console and writes there. Correct for a person typing in a
  terminal, invisible to `cmd | grep`. Not a bug.
- **The MSI is per-user** (`%LOCALAPPDATA%`, no UAC) and the MSVC runtime is
  staged into the package by `MauvelyPackaging.cmake`. Both are shared
  behaviour; see `app-base`.

Verified again 2026-09-09 against **Qt 6.9.0 msvc2022_64**, generator
`Visual Studio 17 2022`, `-DSOTTO_GPU=cpu`. Both tests pass; LayerShellQt and
KF6WindowSystem are absent, which is the "optional pieces missing" case the
build promises to survive.

**The global hotkey does bind now** — `RegisterHotKey` was asked on this machine
and answered. The default used to be `LOGO+ALT+d` on every platform and Windows
*refuses* that one (the shell owns Win+Alt+D for the clock flyout,
`ERROR_HOTKEY_ALREADY_REGISTERED`), so the Windows default is `CTRL+ALT+d`. If
you change it, check the answer rather than assuming: Snap's
`src/platform/hotkeys_win.cpp` has the same warning for the same reason.

Still untested on real Windows hardware: text injection (no focused app to type
into from a render), hold-to-talk key-release polling, and the overlay's
behaviour on a multi-monitor setup.

### Verifying UI changes

There is no display in CI, so the chrome is rendered and looked at:

```bash
QT_QPA_PLATFORM=offscreen SOTTO_SHOT=/tmp/x.png SOTTO_VIEW=settings \
  ./build/bin/Release/MauvelySotto
```

| Variable | Effect |
|---|---|
| `SOTTO_SHOT=<path>` | grab the window after 900 ms, then quit |
| `SOTTO_VIEW=settings\|notepad\|overlay` | which window (default `settings`) |
| `SOTTO_THEME=dark\|light\|system` | force a theme **for this run only** — never written to disk |
| `SOTTO_SIZE=<w>x<h>` | resize before grabbing; the settings page is ~1750px tall and its window is 760 |

`overlay` drives the HUD into `listening` with sample levels and a line of
partial text, because an idle pill is a transparent rectangle. The harness skips
`SingleInstance` on purpose — forwarding "ShowSettings" to a running copy and
exiting would leave you waiting for a file nobody writes — and it quits through
`App::quit()`, without which the windows outlive the QML engine and the run ends
in a page of `TypeError: Cannot read property 'state' of null`.

**Never name a QML property `on` + a capital.** `Theme.onPrimary` is what the
sibling apps' C++ calls the ink on a filled brand surface; in QML that name
collides with the signal-handler syntax, the initialiser is never installed as a
binding, and every reader gets an invalid QColor — which paints **black**. It
shipped as black button labels in both themes and looked deliberate. The
property is `primaryInk`.

## The platform split, and the trap in it

`CMakeLists.txt` gates platform sources as:

```cmake
if(WIN32)
    …WinHotkey…
else()
    …DBusService, PortalRemoteDesktop, PortalRequest, GlobalShortcutsPortal…
endif()
```

**That `else()` is Linux, spelled as "not Windows".** On macOS it compiles the
D-Bus and XDG-portal branch, which is not merely wrong but incoherent — see
`docs/MACOS.md` in app-base, which names this repo as the hard blocker of the
suite's macOS work. Fixing it means a genuine third branch plus new code: a
Carbon or `CGEventTap` hotkey backend and an Accessibility-API text injector.
Guards will not do it.

## Rules & gotchas

- **The only network call is a model download.** Adding a second one changes
  what this product is. If something genuinely needs one, it is a conversation
  before it is a commit.
- **The overlay must never take focus.** On Wayland it is a `wlr-layer-shell`
  surface for that reason; a focusable overlay steals the keyboard from the app
  the text is about to be injected into, which breaks the one thing Sotto does.
- **`LayerShellQt` and `KF6WindowSystem` are optional and auto-disabled.** The
  build must succeed without either — the overlay falls back to a plain
  always-on-top window and the translucency switch just lowers the opacity.
- **whisper.cpp is pinned** to a tag in `FetchContent_Declare`. Moving it is a
  deliberate change with a model-compatibility question attached, not a version
  bump.
- **`SOTTO_STORE_BUILD=ON` compiles the self-updater out.** An MSIX cannot
  replace its own package, so a check whose only possible outcome is "ignore the
  answer" should not be sent.
- **Settings live in a default-constructed `QSettings`.** `main.cpp` calls
  `QSettings::setDefaultFormat(IniFormat)` before anything else, so the file is
  `Mauvely/Sotto.{conf,ini}` — the same pair `apppaths::migrateOrganisation()`
  writes to. Passing an explicit organisation to `QSettings` again would put the
  migration back to copying a file the app then ignores, which is what it did
  from the rename until 2026-09-09.
- **`ModelManager::modelsDir()` is `AppLocalDataLocation`.** Identical to
  `AppDataLocation` on Linux; on Windows that one is the *roaming* profile, and
  `apppaths` builds its paths from `GenericDataLocation`, which is not. Gigabytes
  of model blobs belong in neither a roaming profile nor a directory the
  migration cannot see.

## Handing off

**Each repo has exactly one `HANDOFF.md`, and it is overwritten, not appended.**
It says what the last session did here and what the next one needs to know — not
a changelog. `changes/<hash>.md` and the git log are already the changelog, and a
handoff that grew forever would be a third copy nobody reads.

Read it first. Rewrite it before finishing, if what you did changes what the next
session should know; leave it alone if it does not.

What belongs in it:

- **What changed and why**, at the level a person picking this up cold needs —
  the decision, not the diff.
- **What is half-done**, named precisely enough to resume: the file, the
  function, the thing that is missing.
- **What was tried and rejected**, with the reason. This is the part that saves
  the most time and is almost always missing.
- **What is unverified.** A thing that compiles but has never run should say so.

What does not belong in it: anything already true of the code and readable from
it, anything in `CLAUDE.md` (which is the durable rules, where this is the
transient state), and a list of commits.

## Commit Guidelines

When completing a commit:
1. Write a clear, concise commit message describing the change (no conventional
   commits prefixes needed).
2. Create a markdown file at `changes/<short-commit-hash>.md` containing
   **Changes**, **Additions**, **Bug Fixes** and **Removals**.
3. Include the commit hash in the changes file for reference. This lands as a
   follow-up commit, because a file named after a commit cannot be inside it.
