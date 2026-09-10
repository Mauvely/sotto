# Handoff — 2026-09-10 (late): the overlay's box shadow

Timur: the HUD pill has "a strange shadow of a box around it" — the same
thing Snap's new recording island had. On Windows 11 DWM draws its drop
shadow around the *window's rectangle*, and the overlay window is a
rectangle drawing a capsule inside it. `OverlayController::applyMask()` now
sets the window's region to the capsule (`pillRegion()`, the same shape the
KWin blur-behind region already used) on every resize and show; a regioned
window gets no DWM frame effects, so the shadow goes with the corners. Not
on the layer-shell path, where there is no such window. Built with
`-DSOTTO_GPU=cpu`, `ctest -C Release` 2/2. Seen once on the real display
through a capture: a clean capsule, no box. Not looked at with a pointer.

---

# Handoff — 2026-09-10 (chrome copy pass)

Timur's instruction: cut the unneeded chatter — the LOCAL badge, the "100%
local" / "fully local" / "nothing is sent" reassurances repeated across the
app, and the GPU-backend-reason paragraph at the bottom of Settings. That
reasoning belongs on the product page and in the docs, not in the product.
"The LOCAL badge goes everywhere" meant it is removed everywhere it appeared
(title bar and overlay), not kept — confirmed against the explicit instruction
to delete `qml/LocalBadge.qml`.

Built and verified on Windows: Qt 6.9.0 msvc2022_64, `-DSOTTO_GPU=cpu`,
generator `Visual Studio 17 2022`. `ctest -C Release` is 2/2.

## What changed

- **`qml/LocalBadge.qml` deleted**, along with its two uses
  (`qml/STitleBar.qml`, `qml/Overlay.qml`) and its entry in
  `CMakeLists.txt`'s `QML_FILES`. `Overlay.qml`'s status `Text` used to anchor
  `anchors.right: badge.left`; it now anchors to `parent.right` with a 20px
  margin (matching the logo's 20px on the left). The stale comment in
  `App.cpp::statusText()` that budgeted pill width against "the LOCAL badge"
  was updated to drop the reference (180px → ~210px of room, since the badge's
  width is reclaimed).
- **`qml/SettingsWindow.qml`**: the title-bar subtitle is now "Voice dictation
  for any app." (was "100% local dictation — audio never leaves this
  device."). The Speech model section's explanatory paragraph ("Models run
  entirely on this machine via whisper.cpp…") is now a single "Backend: %1"
  line — the section title already says "Speech model", so the preamble was
  pure restatement. The `App.gpuBackendReason` paragraph and its justifying
  comment are removed from General entirely; the version/backend line above it
  stays. The voice-commands help and the translucency help are each one
  sentence now (the Hyprland `layerrule` fact survives, as one clause).
- **`src/main.cpp`**: `--help`'s banner drops "fully local" (now "Sotto x.y.z
  — voice dictation") and the trailing "All speech recognition runs on this
  machine. Nothing is sent anywhere." line is gone.
- **`src/core/Settings.cpp`**: the Linux `.desktop` `Comment=` is now "Voice
  dictation" (was "Fully local voice dictation").
- **`src/ui/TrayIcon.cpp`**: found by the grep in step 6, not named in the
  original list — a disabled tray menu item read "100% local — nothing leaves
  this device" (the same "constant reminder" pattern as the deleted badge).
  Removed, along with the now-redundant separator around it. The tray tooltip
  changed from "Sotto — local dictation" to "Sotto — voice dictation" for
  consistency with the other trimmed strings.
- **`App::gpuBackendReason()` and `SOTTO_GPU_REASON`** (both `App.h`/`App.cpp`
  and `CMakeLists.txt`) are unchanged — they're useful in a log line, not on a
  page. Nothing in QML references `gpuBackendReason` any more; the accessor
  and the CMake plumbing are kept as instructed. `App::initialize()` now logs
  `qInfo() << "whisper.cpp backend:" << gpuBackendLabel() << "—" <<
  gpuBackendReason()` once at startup.
- **`README.md`**: the intro paragraph's "Fully local…", "every last sample
  … on your machine", and "No accounts, no cloud, no telemetry…" sentences are
  gone — the Privacy section (unchanged) is now the only place the local-only
  claim is made. A short paragraph was added to the Windows section explaining
  that the GPU backend is a build-time choice and that Vulkan specifically
  needs the SDK's `glslc` (a driver alone isn't enough) — this is the content
  that used to be `SOTTO_GPU_REASON`'s home in the UI.
- Grepped `qml/` and `src/` for "local only", "fully local", "100% local",
  "never leaves", "nothing is sent", "on this machine" (case-insensitive):
  zero hits after the above. Log lines and code comments (e.g.
  `ModelManager.h`'s "app's local data" comment, `App.cpp`'s GPU-backend log
  line) were left alone — the instruction only covers user-facing UI copy.

## Verified

- `cmake --build build --config Release --parallel 12` — clean build, exit 0.
- `ctest --test-dir build -C Release` — 2/2 (`formatter`, `speechgate`).
- Offscreen harness, both views, `Qt 6.9.0` build:
  `SOTTO_VIEW=settings SOTTO_SIZE=760x2200` and `SOTTO_VIEW=overlay` — both
  screenshots show no LOCAL badge, no leftover paragraph in General, no
  overlapping chrome. The title bar's subtitle and window buttons have clean
  spacing without the badge; the overlay's status text now runs the full width
  to the pill's right edge.

## Unverified / caveat

- **The new `qInfo()` line in `App::initialize()` did not appear in the
  offscreen harness's captured stderr**, under both Git Bash and a real
  PowerShell console — while the harness's own `std::fprintf(stderr, …)`
  diagnostics (`[shot] wrote …`) did. This matches a pre-existing, undocumented
  property of this binary rather than a defect in the new line: `MauvelySotto`
  is `WIN32_EXECUTABLE` and only calls `attachParentConsole()` for
  `--help`/`--version`/the CLI-control paths (see `main.cpp` and CLAUDE.md's
  Windows section); every other diagnostic that needs to reach a pipe
  reliably — the model-download harness's progress lines — was already written
  with raw `fprintf(stderr, …)` rather than `qDebug`/`qWarning`/`qInfo`, which
  suggests Qt's own message handler doesn't reliably reach an inherited stderr
  handle for a console-less GUI-subsystem process on this machine. The
  existing `qWarning()` calls elsewhere (`App.cpp`, `ModelManager.cpp`,
  `WhisperEngine.cpp`) would have the identical limitation and were not touched
  by this session — this is not new breakage, just newly noticed. Not chased
  further: fixing Qt's console output routing for a GUI-subsystem Windows
  binary is a separate, real piece of work and out of scope for a copy-cleanup
  pass. If startup GPU-backend logging needs to be reliably visible from a
  script, it will need the same `fprintf`-to-an-attached-console treatment the
  harness paths already use, not another `qInfo()`.
- The overlay was only rendered offscreen (`SOTTO_STATE` default =
  `listening`), not looked at on a real display — same caveat as every prior
  session, unchanged by this pass.

## What was tried and rejected

Nothing was tried and reverted this session — the changes were mechanical text
and layout edits with no dead ends.
