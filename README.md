# Sotto

**Fully local voice dictation for Linux.** Press a global shortcut, speak, and the
formatted text lands in whatever app has focus — like Whispr Flow, but every last
sample is processed **on your machine**. No accounts, no cloud, no telemetry:
the only network access Sotto ever performs is downloading a speech model when
*you* ask it to.

- 🎙️ Global shortcut → a small black pill appears at the bottom of the **active**
  monitor with a live waveform and live transcript
- 🧠 [whisper.cpp](https://github.com/ggml-org/whisper.cpp) under the hood, with
  GPU acceleration: **ROCm (AMD)**, Vulkan, or CUDA
- ✍️ Proper writing, not word soup: punctuation and capitalisation from Whisper,
  plus paragraph breaks when you pause, and "new line" / "new paragraph" voice
  commands
- 📥 Text is inserted into the focused app (clipboard+paste or ydotool), or
  copied to the clipboard, or dictated into a built-in notepad window
- ⚙️ Settings UI: model download manager, shortcut, microphone, output method,
  animations toggle, monitor selection
- 🖥️ Designed for **KDE Plasma (KWin) on Wayland** first; the overlay uses
  wlr-layer-shell so it is never focusable and never tiled — Hyprland & friends
  are on the roadmap

## Status

Early v1, built for and tested on: Arch Linux, Plasma 6 / KWin (Wayland), AMD GPU
with ROCm. Expect rough edges elsewhere (see [Roadmap](#roadmap)).

## Requirements (Arch)

```bash
# core
sudo pacman -S --needed base-devel cmake ninja git \
    qt6-base qt6-declarative qt6-multimedia qt6-svg layer-shell-qt

# recommended for inserting text into apps
sudo pacman -S --needed wl-clipboard ydotool
```

GPU backend (pick one):

| Backend | Packages | CMake flag |
|---|---|---|
| **ROCm** (AMD, recommended for you) | `rocm-hip-sdk` | `-DSOTTO_GPU=hip` |
| Vulkan (any GPU; easiest AMD alternative) | `vulkan-headers vulkan-icd-loader glslang shaderc` | `-DSOTTO_GPU=vulkan` |
| CUDA (NVIDIA) | `cuda` | `-DSOTTO_GPU=cuda` |
| CPU only | – | `-DSOTTO_GPU=cpu` |

> **ROCm tip:** if your GPU isn't officially supported by ROCm, export
> `HSA_OVERRIDE_GFX_VERSION` (e.g. `10.3.0` for many RDNA2 cards) before running.
> If the HIP build gives you trouble, the **Vulkan backend is an excellent
> fallback on AMD** — often within a few percent of HIP for Whisper.

## Build

```bash
git clone https://github.com/timurinal/linux-transcription.git
cd linux-transcription
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSOTTO_GPU=hip
cmake --build build -j$(nproc)

./build/sotto            # first run opens Settings
# optional:
sudo cmake --install build
```

whisper.cpp (pinned release) is fetched at configure time; everything is linked
statically into the `sotto` binary.

## First run

1. **Settings** opens automatically. Download a model — **Large v3 Turbo**
   (~1.6 GB) is the recommended default on a discrete GPU; **Small** if you want
   something lighter.
2. KDE will ask to confirm the global shortcut binding (default: **Meta+Alt+D**).
   You can change or disable it any time — the binding also shows up in
   *Plasma System Settings → Shortcuts*.
3. Focus any text field, press the shortcut, talk, press it again. Watch the
   pill at the bottom of your active screen.

### How text gets inserted (Wayland realities)

Wayland has no universal "type this" API, so Sotto picks the best available
strategy (configurable in *Settings → Output*):

| Mode | What happens | Needs |
|---|---|---|
| Clipboard + paste *(default)* | clipboard is set, Ctrl+V is synthesized, old clipboard restored | `wl-clipboard` + (`ydotool` **or** first-time RemoteDesktop portal permission) |
| Type it | text is typed key-by-key through uinput | `ydotool` |
| Clipboard only | text is copied, you paste yourself | `wl-clipboard` |

**ydotool setup** (once):

```bash
sudo systemctl enable --now ydotool   # or run `ydotoold` as your user service
# your user needs write access to /dev/uinput; the ydotool package ships udev rules
```

Terminals usually paste with **Ctrl+Shift+V** — use "Clipboard only" or ydotool
typing there.

### Dictating without inserting

Open the **Notepad** (tray menu or `sotto --notepad`), press *Record*, and the
formatted text accumulates in the window with a Copy button.

### CLI / scripting

```
sotto --toggle     # start/stop dictation (bind this to any compositor shortcut)
sotto --settings   # open settings
sotto --notepad    # open the notepad
sotto --quit
```

A running instance is controlled over D-Bus (`io.github.timurinal.Sotto` at
`/Sotto`): `Toggle`, `Stop`, `ShowSettings`, `ShowNotepad`, `Quit`.

## Formatting

Whisper already produces punctuation and capitalisation. On top of that Sotto:

- inserts a **paragraph break when you pause** longer than a threshold
  (default 2 s, tunable in Settings),
- understands **"new line"** and **"new paragraph"** (toggleable — mind false
  positives like "a new line of products"),
- strips non-speech artifacts (`[BLANK_AUDIO]`, "(laughs)", ♪),
- normalises spacing and sentence capitalisation across utterances.

## Privacy

Sotto is **local-only by design**. Audio never leaves your machine; there is no
server component, no account, and no analytics. Network is used exactly once per
model download, from the official whisper.cpp model repository, at your request.
The `LOCAL` badge on the popup is a constant reminder of that promise.

## Troubleshooting

- **"No speech model installed"** — open Settings and download one.
- **Shortcut doesn't trigger** — your desktop needs the GlobalShortcuts portal
  (Plasma ≥ 5.25 has it). Fallback: disable the shortcut in Settings and bind
  `sotto --toggle` in *System Settings → Shortcuts → Custom*.
- **Nothing is pasted** — install `wl-clipboard`, then either set up ydotool or
  accept the one-time RemoteDesktop permission dialog. Worst case the text is
  always left on the clipboard.
- **Overlay on the wrong monitor** — with `layer-shell-qt` installed and the
  screen setting on *auto*, KWin places it on the active output. Without
  LayerShellQt the fallback window uses the screen under the mouse cursor.
- **Slow transcription** — check the backend line at the bottom of Settings; if
  it says `cpu`, rebuild with `-DSOTTO_GPU=hip` (or `vulkan`).

## Roadmap

- Hyprland / wlroots: layer-shell already works; add `wtype` injection and
  Quickshell-based HUD option
- KWin fake-input backend (no ydotool needed)
- Optional local LLM post-processing pass (llama.cpp) for heavier rewriting
- Streaming decode with whisper.cpp's built-in Silero VAD
- Windows (WASAPI + SendInput), maybe macOS

## License

TBD — currently personal-use software; a proper license will be chosen before
any wider release.
