# cedb624 — Sotto builds and runs on Windows, and wears the suite's chrome

Commit `cedb624`. `HANDOFF.md` carries what is unverified.

## Changes

- Windows default shortcut Ctrl+Alt+D; settings in `%APPDATA%/Mauvely/Sotto.ini`; models under the local app data directory.
- `qml/Theme.qml` mirrors the native theme values, with a Theme setting (system, dark, light); sections are `SPanel` regions on the tinted ground; the design system durations behind an Animations setting.

## Additions

- `qml/{Theme,SPanel,SRadio,SCheckBox,SSwitch,SComboBox,STextField,SSlider}.qml`; the `SOTTO_SHOT` harness; `HANDOFF.md`.

## Bug Fixes

- The global shortcut could never bind on Windows.
- The organisation rename migrated settings nobody read.
- `Theme.onPrimary` painted black in both themes.
- A blocked paste was reported as inserted.
- "Apply && re-bind" rendered literally.
- The screenshot run quit through the wrong path and crashed on exit.

## Removals

- None.
