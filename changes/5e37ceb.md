# 5e37ceb — Linux CI could not install libfuse2

## Bug Fixes

- **`apt-get install libfuse2` fails on Debian 13 and Ubuntu 24.04.** The
  package was renamed `libfuse2t64` in the 64-bit `time_t` transition, so the
  "Install Linux GUI dependencies" step failed with `E: Unable to locate package
  libfuse2` and took the whole Linux lane with it.

  Not a self-hosted-only fault: `ubuntu-latest` has been both releases. The
  self-hosted runner simply reached the new one first.

## Changes

- `.github/workflows/ci.yml` and `.github/workflows/release.yml` drop `libfuse2`
  from the main package list and install it separately as
  `libfuse2t64 || libfuse2`, so one workflow serves both naming eras.
- Not `|| true`. The package is really needed by the AppImage tooling, and
  swallowing the failure would surface it later in a step whose message names
  nothing to do with FUSE.

## Additions

- Nothing added.

## Removals

- Nothing removed.

## Verification

Confirmed on the runner: `apt-cache policy libfuse2` reports no candidate,
`libfuse2t64` is installed at 2.9.9-9. All 17 workflows across the eight repos
re-parse as valid YAML after the edit.
