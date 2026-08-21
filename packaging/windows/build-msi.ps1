<#
.SYNOPSIS
    Builds the per-user .msi installer for a Mauvely desktop app.

.DESCRIPTION
    CPack's WiX generator does the packaging; this script exists for the part
    CPack does not do — proving the package is not broken.

    ── The failure this is here to catch ───────────────────────────────────────

    Compose shipped an .msi that installed cleanly and then died before main()
    with:

        The code execution cannot proceed because Qt6Widgets.dll was not found.

    Its CI *did* have a deployment check. The check looked at build/bin/Release,
    which is the build tree — CPack never reads that — so it passed for every
    build while every .msi shipped a lone executable. A deployment check is only
    worth anything if it inspects the tree that becomes the package, which is
    CPack's staging directory under build/_CPack_Packages/.

    ── Requirements ────────────────────────────────────────────────────────────

    WiX Toolset v3 (candle.exe/light.exe). GitHub's windows-latest runners do not
    ship it: `choco install wixtoolset -y --no-progress`.

.PARAMETER BuildDir
    The configured CMake build directory (e.g. ./build).

.PARAMETER OutFile
    Optional. Copy the produced .msi here under the release naming convention
    <Binary>-<version>-windows-x86_64.msi.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [string]$OutFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$infoPath = Join-Path $BuildDir 'app-info.json'
if (-not (Test-Path $infoPath)) {
    throw "$infoPath not found — configure the project first (cmake -S . -B $BuildDir)."
}
$info    = Get-Content -Raw $infoPath | ConvertFrom-Json
$binary  = $info.binary
$version = $info.version

# ── 1. build the package ────────────────────────────────────────────────────

Write-Host "Packaging $binary $version with CPack/WiX"
& cmake --build $BuildDir --config Release --target package
$packExit = $LASTEXITCODE

# CPack reports "Problem running WiX. Please check ... wix.log" and then exits,
# so the only real diagnostics — candle/light errors, ICE validation failures —
# are in a file nobody ever sees on a hosted runner. Print it before throwing.
if ($packExit -ne 0) {
    Get-ChildItem -Path (Join-Path $BuildDir '_CPack_Packages') -Filter 'wix.log' `
        -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "===== $($_.FullName)"
        Get-Content $_.FullName | ForEach-Object { Write-Host $_ }
    }
    throw "cpack failed with exit code $packExit."
}

# ── 2. verify the staged tree, not the build tree ───────────────────────────

$stage = Get-ChildItem -Path (Join-Path $BuildDir '_CPack_Packages') -Directory -Recurse `
             -ErrorAction SilentlyContinue |
         Where-Object { Test-Path (Join-Path $_.FullName "bin\$binary.exe") } |
         Select-Object -First 1
if (-not $stage) {
    Write-Host "::error::No CPack staging directory holding bin\$binary.exe was found."
    Get-ChildItem -Recurse (Join-Path $BuildDir '_CPack_Packages') -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
    throw 'Could not locate the staged install tree.'
}
$bin = Join-Path $stage.FullName 'bin'
Write-Host "Staged install tree: $bin"

$required = @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll')
$hasWebEngine = Test-Path (Join-Path $bin 'Qt6WebEngineCore.dll')
if ($hasWebEngine) { $required += @('Qt6WebEngineWidgets.dll', 'QtWebEngineProcess.exe') }
if (Test-Path (Join-Path $bin 'Qt6Sql.dll')) { $required += 'sqldrivers\qsqlite.dll' }

$missing = @($required | Where-Object { -not (Test-Path (Join-Path $bin $_)) })
if ($hasWebEngine -and
    -not (Get-ChildItem -Path $bin -Recurse -Filter '*.pak' -ErrorAction SilentlyContinue)) {
    $missing += 'qtwebengine_resources.pak (resources\)'
}
if ($missing.Count -gt 0) {
    Write-Host "::error::The .msi would install an app that cannot start — missing runtime files."
    $missing | ForEach-Object { Write-Host "  missing: $_" }
    Get-ChildItem -Recurse $bin | Select-Object -ExpandProperty FullName
    throw 'Qt runtime incomplete in the staged install tree.'
}
Write-Host ("Qt runtime staged: {0} files." -f (Get-ChildItem -Recurse -File $bin).Count)

# Reported, not enforced. A per-user MSI cannot install the VC++ redistributable
# — that needs admin — so if these are absent the app relies on the machine
# already having it. Worth deciding on this listing rather than on a guess about
# windeployqt's default.
@('vcruntime140.dll', 'msvcp140.dll') | ForEach-Object {
    Write-Host ("  MSVC runtime {0}: {1}" -f $_, (Test-Path (Join-Path $bin $_)))
}

# ── 3. name it the way the release feed indexes it ──────────────────────────

$msi = Get-ChildItem -Path $BuildDir -Filter '*.msi' -File |
       Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $msi) { throw "cpack reported success but produced no .msi in $BuildDir." }

if (-not $OutFile) {
    $OutFile = Join-Path $BuildDir "$binary-$version-windows-x86_64.msi"
}
$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }
if ($msi.FullName -ne (Resolve-Path -LiteralPath $OutFile -ErrorAction SilentlyContinue)) {
    Copy-Item -LiteralPath $msi.FullName -Destination $OutFile -Force
}

$size = (Get-Item $OutFile).Length
Write-Host ("Wrote {0} ({1:N1} MB)" -f $OutFile, ($size / 1MB))
