# Handoff — 2026-09-10

Five things found by running Sotto on Windows after the 2026-09-09 pass, plus
one more the running turned up. All of it landed the same day in one commit,
with its `changes/` note behind it.

Everything here was built with Qt 6.9.0 msvc2022_64, `-DSOTTO_GPU=cpu`,
generator `Visual Studio 17 2022`. `ctest -C Release` is 2/2. Unlike the last
session, some of this **was** seen on a real display and driven with real Win32
messages; what was and was not is stated per item below.

```sh
cmake -S . -B build -G "Visual Studio 17 2022" \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64 -DSOTTO_GPU=cpu
cmake --build build --config Release --parallel 24
ctest --test-dir build -C Release          # 2/2
```

## 1 — Sotto has the suite's window chrome now

It used the OS title bar. It now draws its own, in a frameless window, with the
Windows native-frame takeover — the same behaviour Compose, Snap, Relay and
Suite get from `app-base/src/ui/framelesswindow.{h,cpp}`.

**`src/ui/framelesswindow.h` is not that file and must never be confused with
it.** app-base's is a QWidget and is one of four byte-identical copies pinned by
the superproject's table; Sotto's UI is QML, so this is a **QQuickWindow
subclass** (`SottoWindow`, `QML_ELEMENT`, used as the root of
`SettingsWindow.qml` and `NotepadWindow.qml`). The Windows half is ported line
for line and for the stated reasons: `kFrameStyles` put back over Qt's
`WS_POPUP`, `WM_NCCALCSIZE` taking the frame's pixels instead of its styles,
`WM_NCHITTEST` handing back the caption band and the resize edges, `HTMAXBUTTON`
so Windows 11 opens Snap Layouts, `autoHideEdge()`'s one pixel, and a
`toggleMaximized()` that calls `ShowWindow(SW_MAXIMIZE)` rather than Qt.

QML tells C++ where the regions are, because only QML knows:
`captionHeight`, `captionExclusions` (the window buttons), `maximizeButton`,
`borderColor` (fed to `DWMWA_BORDER_COLOR` from `Theme.borderStrong`).

**Measured, not assumed.** A real run driven by `SendMessage` from PowerShell:

```
style 0x16CF0000 — WS_POPUP False, WS_CAPTION/WS_THICKFRAME/WS_MAXIMIZEBOX True
hit-test maximise button -> HTMAXBUTTON (9)     bare title bar -> HTCAPTION (2)
hit-test close/min button-> HTCLIENT   (1)      left edge      -> HTLEFT (10)
                                                bottom-right   -> HTBOTTOMRIGHT (17)
WM_NCLBUTTONDOWN/UP with HTMAXBUTTON:
  rect -8,-8 2576x1456   IsZoomed True   (work area 0,0 2560x1440)
  and back to 960,340 640x760, IsZoomed False
```

That `-8,-8 2576×1456` is exactly the shape app-base's comment describes and the
one a drag to the top edge produces.

**The overlay HUD is untouched** — it is a layer-shell/always-on-top pill with
no chrome, and it must stay that way.

**Not verified:** anything outside Windows. The non-Windows path is frameless
plus `startSystemMove()`/`startSystemResize()` with `qml/SResizeEdges.qml`'s
eight grips and a QML-painted 1px edge, which is what app-base does on Wayland
and X11 — but it has never been compiled or run on Linux from here. The windows
are **square** off Windows (DWM rounds them on Windows); the design's 12px
corners would need a translucent window, which is a judgement to make on a real
Linux desktop rather than blind.

## 2 — The download that "did nothing"

`ggml-large-v3.bin.part` was on disk at **0 bytes**, created 2026-09-09 23:48:51
and never written to, beside a `ggml-large-v3-turbo.bin` that had completed
36 seconds earlier. So an attempt was made and not one byte arrived.

**The cause was not reproducible and is not named here — do not believe anyone
who says it was.** Checked and ruled out: the URL (both models 302 to
`us.aws.cdn.hf.co` and serve ranged GETs fine), the redirect policy, disk space
(1.3 TB free), the models directory, and HTTP/2 — Large v3 completes with Qt's
HTTP/2 both allowed and forbidden, tested A/B.

What *was* certainly broken is that none of it could ever have been visible:

- **`downloadFinished(id, ok, error)` had no listener anywhere in the QML.** A
  failed download removed its progress bar, restored the Download button and
  said nothing at all.
- `QFile::write`'s return value was dropped, so a short write produced a
  truncated file that failed its size check at the end and was reported as "not
  a valid ggml model" — pointing the reader at the wrong thing entirely.
- Nothing watched a reply that connected and then delivered nothing. There was
  no timeout, no byte counter and no retry, so 0% forever was a reachable state
  with no exit and no message.

So `ModelManager` was rebuilt around the assumption that a multi-gigabyte
transfer will sometimes die:

- a **45-second stall watchdog** on bytes-received, not on wall time;
- **three attempts**, each resuming with `Range: bytes=N-` from what is already
  in the `.part` (the CDN advertises `accept-ranges: bytes` and answers 206);
- the `.part` is **kept** on a network failure or an early end, and pressing
  Download again resumes from it — it is only deleted when it cannot be
  continued (bad ggml magic, or an explicit Cancel);
- a byte readout (`Models.downloadStatus`, "1.49 GB / 3.10 GB") beside the bar,
  and a progress denominator that falls back to the catalog size when there is
  no `Content-Length`;
- free-space checked *before* the download rather than after;
- and the error is shown, once, in the Speech model panel.

**Verified end to end against the real endpoint:** Tiny 77,691,713 bytes in 2 s,
exit 0. Large v3 downloaded, killed at 1.48 GB, re-run, **resumed** and finished
at 3,095,033,483 bytes — the exact expected size — exit 0. Both are installed on
this machine now, along with Turbo. (`ggml-tiny.bin` was left in place: on a
CPU-only build it is the only model that transcribes at a sane speed and is the
right thing to test dictation with.)

The headless harness that made this testable is `SOTTO_DOWNLOAD` — see
CLAUDE.md § Verifying UI changes.

## 3 — The drop-downs drew empty rows, for two separate reasons

Both in `qml/SComboBox.qml`, and either alone was enough.

1. The delegate took its text from the shape in Qt's own ComboBox
   *customisation example*: `Array.isArray(control.model) ? modelData[textRole]
   : model[textRole]`. Every combo whose model is a JS array of objects —
   language, behaviour, injection mode, theme — went down the `modelData` branch
   and got `undefined`. The console said so on every row ("Unable to assign
   [undefined] to QString") and nobody had looked, because a popup is the one
   piece of chrome a screenshot of the settled window does not contain. It is
   `control.textAt(index)` now, which is ComboBox's own C++ resolution and is
   right for a plain string list and a role-bearing object alike.
2. `ItemDelegate` defaults to `padding: 12`, and the delegate is pinned to
   `height: 30` — 6px of availableHeight, into which a `Text` with `elide` set
   draws **nothing** rather than overflowing. Every other `S*` control in the
   repo already sets `padding: 0`; this one did not.

`SOTTO_OPEN=combo` opens the language drop-down before the grab, so this is
checkable from now on. Both rules are in CLAUDE.md.

## 4 — The backend says CPU, and now says why

**This machine has no GPU toolchain and none was installed.** Probed: no
`nvcc`, no `CUDA_PATH`, no `C:\Program Files\NVIDIA GPU Computing Toolkit`; no
`VULKAN_SDK`, no `C:\VulkanSDK`, no `glslc`. There *are* two GPUs (Radeon RX
9070 XT, GeForce GTX 1060) and a working Vulkan 1.4 runtime — `vulkaninfo`
enumerates the Radeon — but ggml-vulkan compiles its own shaders and needs the
SDK's `glslc`, so a driver is not enough. The build stays on `cpu`.

`CMakeLists.txt` now probes for both at configure time
(`find_package(Vulkan COMPONENTS glslc)`, `find_package(CUDAToolkit)`) and bakes
the answer into `SOTTO_GPU_REASON`, which the settings page prints verbatim
under the backend line. If a toolchain *is* found and the build is still `cpu`,
it says so and names the flag instead. The one thing a reader must not be left
thinking is that the app failed to find their card at run time.

**To get a GPU build here:** install the LunarG Vulkan SDK, then
`cmake -S . -B build -DSOTTO_GPU=vulkan` and rebuild. Vulkan rather than CUDA
because it covers both cards and the Radeon is the faster one.

Two traps in the CMake, both hit: a `;` inside an unquoted `${VAR}` expansion is
a list separator and split the `-D` into half a string literal ("newline in
constant"), and the whole definition must be one quoted argument. The string is
ASCII on purpose — it ends up on an MSVC command line.

## 5 — "Formatting" was never the formatter

`Overlay.qml` printed "Formatting…" for the whole of the `finalizing` state.
`TextFormatter::format()` is a handful of regular expressions over a few
kilobytes and has never taken measurable time. What `finalizing` actually waits
for is **whisper decoding the utterances committed while the person was still
speaking**, and on a CPU-only build with Large v3 that is minutes. The label
named the cheap step and hid the expensive one.

Four things were wrong underneath it, and all four are fixed:

- **A final decode queued behind a partial nobody wanted.** Requests are
  serialized FIFO on the worker, and `finalizeUtterance()` left the in-flight
  partial running — a preview of the very utterance about to be decoded
  properly. `TranscriptionSession` now emits `dropStalePartials(id)` on every
  commit and on `end()`, wired to `WhisperEngine::dropPartialsBefore()` with a
  **direct** connection: the worker may be *inside* `whisper_full()`, so it has
  to be a relaxed atomic the ggml `abort_callback` reads, not a queued slot the
  worker could not reach until the decode it is meant to stop had finished.
  Queued-but-stale partials are dropped at the top of `transcribe()` too.
- **The partial cadence ignored what a partial cost.** `partialIntervalMs` was a
  schedule; it is a floor now, and the real floor is the last decode's own
  elapsed time (`transcribed()` carries it). A machine where a partial takes
  three seconds asks for one every three seconds.
- **`n_threads` was capped at 8** — sensible on a small machine, half the
  hardware on this one. Now `clamp(hardware_concurrency - 2, 2, 16)`: two cores
  left for the GUI and the audio callback, and 16 is where whisper's own scaling
  flattens.
- **There was no way out.** A second press of the shortcut during `finalizing`
  did nothing; it now calls `TranscriptionSession::finishNow()`, which takes
  what has decoded and stops waiting for the rest.

The UI says what is happening: `App::statusText()` is the single source for the
HUD, the notepad's transport strip and anything added later — "Transcribing…
2 left", then "Transcribing… 47%" (from whisper's `progress_callback` on the
final pass), then "Formatting…" only for the step that is actually the
formatter. `SOTTO_STATE=finalizing` renders it.

**Unverified, and this is the important line in this file: none of §5 has been
through a microphone.** There is no audio input on this machine and no
transcription has ever run here. The cancellation, the backoff, the progress
percentage and the countdown all compile, and the state they drive renders
correctly offscreen — but the numbers in that countdown have never come from a
real decode. `test_formatter` and `test_speechgate` do not touch whisper.

## 6 — Found by running: there was no single instance on Windows

Not one of the five. Two `MauvelySotto --settings` both stayed running — two
tray icons, and the second `RegisterHotKey` losing to the first.

`SingleInstance::registerPrimary()` used `QLocalServer::listen()` as the lock,
on the reasoning that a Windows named pipe dies with its process so a failed
listen means a live instance. The premise is right and the conclusion is not:
Qt creates the pipe with `PIPE_UNLIMITED_INSTANCES` and **without**
`FILE_FLAG_FIRST_PIPE_INSTANCE`, so a second server on the same name simply gets
another instance of it and `listen()` succeeds.

A named mutex (`Local\net.mauvely.sotto.instance`) is the lock now; the pipe
stays as the transport. And `--quit`/`--stop` with nothing running used to fall
through every branch in `main()` and start a whole background instance in order
to honour a request to stop — they return 0 instead.

Measured before: two instances alive, `--quit` never returned and the primary
never exited. After: second instance exits, `--quit` shuts the running one down
in 83 ms, no leftovers.

## Still broken, or unverified

- **No microphone, no decode, ever.** See §5. Audio capture, segmentation, the
  whisper worker thread and text injection are all still unexercised here.
- **Nothing outside Windows has been compiled this session.** The frameless
  path, `SResizeEdges`, and the `#else` branches in `framelesswindow.cpp` are
  Linux-untested. `src/core/SingleInstance.cpp`'s Windows branch now includes
  `<windows.h>`; the Linux branch gained only a defaulted destructor.
- **`src/ui` is on the include path now**, and only because qmltyperegistrar
  records a `QML_ELEMENT` header by its **bare filename** in the metatypes JSON
  and then emits `#if __has_include(<framelesswindow.h>)` around the include it
  needs — when that does not resolve the guard silently skips it and the
  registration below fails to compile on an undeclared type. Setting
  `CMAKE_AUTOMOC_PATH_PREFIX` does not help (it changes moc's own `#include`,
  not the JSON). Repo-rooted includes are still the convention; this is a build
  workaround, not a licence to write `#include "framelesswindow.h"`.
- **The theme has still never been toggled at runtime.** Two more `Canvas`
  items were added (`SWindowButton`'s glyph); like the logo arcs and the
  checkbox tick it repaints on an explicit `onStrokeChanged`, because a Canvas
  keeps its pixels. A third one needs the same.
- **`App::quit()` is still the only orderly shutdown**, and the windows are
  still parentless. Unchanged from the last session.
- **Nothing pins the tokens.** Still the single most valuable thing left: a test
  cannot reach `Theme.qml` because the QML module is built into the executable
  by `qt_add_qml_module` rather than into a library a test could link. Splitting
  it out is the prerequisite. The `SComboBox` bug is a second argument for it —
  a QML unit test would have caught an empty delegate label in a second.
- **Existing installs.** Still none on Windows.

## What was tried and rejected

- **Blaming HTTP/2 for the Large v3 stall.** It was a good hypothesis — Turbo
  (1.6 GB) had completed and Large v3 (3.1 GB) had not, and Qt's HTTP/2 path is
  the less travelled one — and it is wrong. Tested both ways; Large v3 completes
  either way. `Http2AllowedAttribute` is *not* set: leaving Qt's default in
  place is honest, and a comment naming a cause that was disproved would be
  worse than no comment.
- **`tr("… %n passage(s) left", nullptr, n)`.** The `(s)` plural markup is a Qt
  Linguist convention that only a loaded translation resolves, and Sotto has no
  translations at all yet — it rendered literally as "2 passage(s) left" in the
  HUD. The branch only runs for n > 1, so the plural is unconditional.
- **`elide: Text.ElideLeft` for the HUD's status line.** Right for live
  transcript (keep the newest words) and wrong for a status: it delivered
  "…scribing… 2 left". The elide direction now follows which of the two the
  label is showing.
- **A `SOTTO_STATE` harness variable** was tried and rejected last session,
  because the HUD had nothing to draw outside `listening`. It draws a countdown
  now, so it is back — but only for `overlay`, and only `finalizing`.
- **`CMAKE_AUTOMOC_PATH_PREFIX ON`** as the fix for the qmltyperegistrar include
  — see above. It changes the wrong path.
