# fe29e5e — Three reasons a Windows release could not finish

None of the three is in an app. All three are in the release lane, and each
one hid the next: the build error was reachable only once the packaging error
was, and the packaging error only mattered because a failed configure was not
a failure.

## Bug fixes

### A backspace character in the Inno Setup path

`scripts/package.ps1` calls three packaging scripts. Two are correct. The
third was written with its `\b` interpreted as an escape, so the file holds
byte `0x08` where the separator belongs, and PowerShell reported exactly what
it found:

```
The term 'D:\a\...\packaging\windows<BS>uild-inno.ps1' is not recognized as a
name of a cmdlet, function, script file, or executable program.
```

By then the MSI had been built, the tree staged, the MSVC runtime verified and
a 126 MB installer written. The `.exe` step failed, took the job with it, and
a release that had done all of its work published nothing.

The line is byte-identical again in all eight repositories.

### Splatting one flag passed it one character at a time

```powershell
$flags = $env:CONFIGURE_FLAGS -split '\s+' | Where-Object { $_ }
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release @flags …
```

A PowerShell pipeline that yields **one** element returns that element, not a
one-element array. `@` on a string enumerates it, so cmake was handed

```
- D C E _ R E Q U I R E _ W E B E N G I N E = O N
```

and said `Unknown argument -`. Compose, Play, Relay and Sotto have exactly one
flag in `CONFIGURE_FLAGS`; app-base, Mail, Snap and Suite have two or three
and worked only by accident. `@(…)` makes it an array at one element or twenty.

What was actually lost is the guard: the configure that failed is the one
carrying `-D*_REQUIRE_*=ON`, the flags whose whole job is to fail the build
when an optional component is missing rather than ship without it.

### A failed native command was not a failed step

`$ErrorActionPreference = 'Stop'` governs cmdlets, not native commands, so
cmake could exit non-zero and the step would carry on and build whatever the
previous configure had left in the cache. That is why the splat survived every
Windows release without anybody seeing it.
`$PSNativeCommandUseErrorActionPreference = $true` closes it.

### The feed check could never pass

```bash
printf '%s' "$RESPONSE" | python3 - "$V" <<'PYEOF'
…
items = json.load(sys.stdin)["items"]
```

The heredoc *is* stdin — it is where python reads its script from — so the
piped response reached nothing and `json.load` read an exhausted stream. Every
run of this step since it was written ended on

```
json.decoder.JSONDecodeError: Expecting value: line 1 column 1 (char 0)
```

whether or not the release had published. The response travels in the
environment now. The gate's own copy of this pattern uses `python3 -c`, which
leaves stdin free, and was always correct — which is why one worked and one
did not.

## Changes

- `finalise` now requires `needs.gate.result == 'success'` as well as its
  `should_release` output. A gate that fails has still set its outputs by the
  time the `if` is evaluated, so one missing release secret produced two
  failure emails: the real one, and this job failing to verify a version that
  nothing had built.

## What was run

The splat was reproduced and the fix confirmed in PowerShell at one, three and
zero flags — a single flag reaches a native command as `- D C E …` and reaches
it whole once wrapped. The heredoc collision was reproduced at a shell and
produces the identical `JSONDecodeError`. `package.ps1` parses cleanly, and all
eight copies are byte-identical again. The workflows parse as YAML.
