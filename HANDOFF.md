# Handoff — 2026-09-09

Sotto had not been built or run for a while and never properly on Windows. It
was brought up on Qt 6.9 / MSVC, five bugs were found and fixed, and the UI was
moved onto the suite's 2026-09-09 chrome decision (tinted ground, panels,
motion) plus a light theme it did not have.

**Nothing below has been seen on a real display, and no audio has been through
it.** Everything visual was checked by rendering offscreen and reading pixels.

## Building it here

```sh
cmake -S . -B build -G "Visual Studio 17 2022" \
      -DCMAKE_PREFIX_PATH=C:/Qt/6.9.0/msvc2022_64 -DSOTTO_GPU=cpu
cmake --build build --config Release --parallel 24
ctest --test-dir build -C Release          # 2/2
```

Qt 6.9.0 msvc2022_64 works; 6.7.3 msvc2019_64 is also on this machine and was
not needed. LayerShellQt and KF6WindowSystem are absent on Windows, which is the
"optional pieces missing" case the build promises to survive — it does, first
try, with no warnings beyond Qt's own QTP0004 policy note.

## What was broken, and is now fixed

**The global shortcut could never bind on Windows.** The default was
`LOGO+ALT+d` on every platform, and the Windows shell owns Win+Alt+D (the clock
and calendar flyout) — `RegisterHotKey` refuses it with
`ERROR_HOTKEY_ALREADY_REGISTERED`. The one thing the app is for was dead on
first run, and the only report was a line of small print in Settings. Asked
Windows directly on this machine: Win+Alt+D and Ctrl+Alt+Space are refused;
Ctrl+Alt+D, Ctrl+Shift+D, Win+Shift+D, Win+Alt+S and Win+Alt+V are free. The
Windows default is now `CTRL+ALT+d` (`Settings::preferredShortcut`), Linux is
unchanged, and a settings render confirms **"Currently bound: Ctrl+Alt+D"**.

**The organisation rename never reached the settings file.** `main.cpp` calls
`apppaths::migrateOrganisation("sotto", "sotto")` and `Settings` then constructed
`QSettings(IniFormat, UserScope, "sotto", "sotto")` — the *pre-rename* pair. So
the migration ran on every launch, shuffled keys between two locations nobody
read (NativeFormat: the registry, on Windows), and the app carried on reading
the old file. `main.cpp` now sets `QSettings::setDefaultFormat(IniFormat)` before
the migration and `Settings` default-constructs its `QSettings`, so both halves
land on `Mauvely/Sotto.{conf,ini}`. Verified: a real run wrote
`%APPDATA%/Mauvely/Sotto.ini`.

**Downloaded models were invisible to that migration on Windows.**
`ModelManager::modelsDir()` used `AppDataLocation` — the *roaming* profile —
while `apppaths` builds its paths from `GenericDataLocation`, which is
`%LOCALAPPDATA%`. Identical on Linux, different on Windows, so a rename would
have stranded gigabytes and reported every model uninstalled. Now
`AppLocalDataLocation`, which is also simply the right home for model blobs.

**`Theme.onPrimary` painted black.** In QML a property named `on` + a capital
collides with the signal-handler syntax: the initialiser is never installed as a
binding and every reader gets an invalid QColor. It is the ink on every filled
brand surface, so all the primary button labels, the radio dots, the checkbox
ticks and the switch knobs rendered pure black in **both** themes — and black on
violet 400 looks deliberate. Found by sampling pixels, not by looking. The
property is `Theme.primaryInk`; the rule is in CLAUDE.md.

**Windows paste reported success it had not had.** `sendPasteKeystroke()`
discarded `SendInput`'s return value, so when Windows blocked the input (UIPI —
the focused window belongs to a more privileged process) the app said the text
had been inserted. It now returns the answer, and the failure message no longer
tells a Windows user to set up ydotool.

**A screenshot run ended in a page of QML errors.** The harness quit through
`QCoreApplication::quit()`, so the windows outlived the QML engine and every
binding on the `App`/`Config` context properties re-evaluated against a null. It
quits through `App::quit()` now. The same shape of error is still reachable on a
normal exit that does not go through `App::quit()` — see *Still broken* below.

Also: the Settings button read **"Apply && re-bind"**. `&&` is the QWidget escape
for a literal ampersand and Qt Quick does not parse mnemonics at all.

## What changed visually

There was no screenshot harness at all, so the first job was building one —
`SOTTO_SHOT` / `SOTTO_VIEW` / `SOTTO_THEME` / `SOTTO_SIZE`, documented in
CLAUDE.md § Verifying UI changes.

**A light theme, and a theme setting.** Sotto was dark-only: `Brand.qml` holds
one set of text and ground roles, all of them the dark ones. `qml/Theme.qml` is
new — the semantic layer, resolved from `Config.darkMode`, mirroring
`snap/src/core/theme.cpp` value for value so the two apps agree about what
"panel" means. `Settings ▸ Appearance ▸ Theme` is *Match the desktop* (the
default, via `QStyleHints::colorScheme`), *Dark* or *Light*.

**Panels on a tinted ground.** The settings window's ground is `Theme.bg`
(`#12142e` / `#e8e5f9`) and every `SSection` is now a 16px-radius panel on it,
16px apart, with the header row sitting on the ground as this window's title bar.
`SSection` inherits from the new `SPanel`, so the settings window's structure
barely moved. The notepad is two panels: the editor and a tinted transport strip.
The HUD pill is `Theme.chromePanel` — it is the only chrome Sotto has, and the
decision is that chrome is tinted.

**The Basic style's controls were replaced.** `RadioButton`, `CheckBox`,
`Switch`, `ComboBox`, `TextField` and `Slider` were unstyled — flat mid-grey
discs and boxes that belonged to neither theme and were the loudest wrong thing
in the old render. `qml/S{Radio,CheckBox,Switch,ComboBox,TextField,Slider}.qml`
are brand skins over them. The checkbox tick and the combo chevron are stroked
on a `Canvas` with Lucide's 2px round-cap geometry rather than typed as "✓" and
"▾", which the design system rules out.

**Motion.** Every duration goes through `Theme.durFast` / `durBase` / `durSlow`,
which return **0** when `Config.animationsEnabled` is off — no second code path,
and that is also the reduced-motion behaviour the design system asks for. Hover
and press colour on every control is `durFast`; the switch knob slides at
`durFast`; the HUD's entrance is a fade plus a 3% scale at `durSlow`; panel fills
cross-fade at `durBase` so a theme toggle moves together. The Appearance switch
was relabelled from "Animations in the dictation popup", which is no longer what
it governs.

**Two design-system rules were deliberately not followed**, both with a reason
in the code:

- **The HUD has no 4px rise.** The window *is* the pill and `OverlayController`
  derives the KWin blur region from the window's geometry, so a pill that moves
  inside its window would be clipped at the bottom and blurred through a capsule
  it no longer fills.
- **The notepad's text area paints no ground.** A sunken well filling a panel
  edge to edge draws the same region twice; the inset treatment is for a field
  among other things, not for the one piece of content the window exists to show.

## Still broken, or unverified

- **Nothing has been seen on a display.** Both themes were rendered offscreen
  and read, including sampling pixels to confirm the grounds. That is not the
  same as looking at it. In particular the light theme's tinted transport strip
  (`#f5f3ff`) against the editor panel (`#f8f9fd`) is four levels apart — Snap's
  handoff flags the same pair as needing a judgement on a real screen.
- **The theme has never been toggled at runtime.** Every colour is a binding on
  `Config.darkMode`, so it should follow, but each render is a fresh process
  with `SOTTO_THEME` set. The two `Canvas` items (the logo arcs, the checkbox
  tick) repaint on an explicit `onColorChanged` because a Canvas keeps its
  pixels; if a third one is added it needs the same.
- **No microphone, no model, no decode.** `ctest` is 2/2 —
  `test_formatter` and `test_speechgate`, neither of which touches whisper.
  Audio capture, segmentation, the whisper worker thread and text injection are
  all still unexercised on Windows.
- **Hold-to-talk release polling is untested.** `WinHotkey::pollRelease()` only
  watches the main key, not the modifiers.
- **`App::quit()` is the only orderly shutdown.** Anything that ends the event
  loop without it leaves the QML windows outliving the engine, which prints a
  page of `TypeError: Cannot read property 'state' of null`. The windows are
  also parentless (`QQmlComponent::create()` gives them no owner), so that path
  leaks them. `TrayIcon`'s `QMenu` is parentless too — `setContextMenu` does not
  take ownership.
- **Nothing pins the tokens.** Sotto has no `core_smoke`, and the sibling apps'
  equivalent assertions (`panel() != bg()`, five distinct grounds, `duration()`
  is 0 with the setting off) would have caught the black `primaryInk` in a
  second. A test cannot reach `Theme.qml` today because the QML module is built
  into the `MauvelySotto` executable by `qt_add_qml_module`, not into a library a
  test binary could link. Splitting it out is the prerequisite, and it is the
  single most valuable thing left here.
- **`Theme.panelAlt` is defined and unused.** Sotto has no strips inside panels
  yet. Kept because it is the design system's table, not because something needs
  it.
- **Existing installs.** There are none on Windows (no MSI published), and the
  settings and models moves are both handled by `apppaths` on Linux — but a
  Linux user upgrading across this change is the one case that has not been
  tried, and the settings half of that migration has never actually run.

## What was tried and rejected

- **Copying `src/ui/panel.{h,cpp}` from Snap.** It is a QPainter helper for
  QWidget code. Sotto is QML; `qml/SPanel.qml` carries the same values.
- **Fixing the migration in `src/core/apppaths.cpp`.** That file is
  byte-identical across all eight native repos (`git hash-object` agrees:
  `61e7f8b…`) and is not in the superproject's shared-files table, so a change
  there would be an invisible drift. Both fixes are on Sotto's side of the line
  instead.
- **A `SOTTO_STATE` harness variable.** The HUD only has something to draw while
  listening, so `SOTTO_VIEW=overlay` just drives it there.
