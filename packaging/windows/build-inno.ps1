<#
.SYNOPSIS
    Builds the direct-download .exe installer for a Mauvely desktop app.

.DESCRIPTION
    The third Windows format, beside the .msi and the .msix. It exists because
    "run the .exe" is what most people expect a download button to give them —
    the MSI is the more correct package and the MSIX is the Store's, and neither
    is what somebody clicking Download is looking for.

    ── What it packages ────────────────────────────────────────────────────────

    The same staged tree the .msi is built from: CPack's directory under
    build/_CPack_Packages/. Not the build tree.

    That is not an implementation detail. Compose once shipped an .msi that
    installed cleanly and died before main() on a missing Qt6Widgets.dll,
    because its deployment check looked at build/bin/Release — a directory CPack
    never reads. Building this from the same verified tree means the .exe and
    the .msi carry byte-identical payloads and cannot drift apart.

    So this script runs *after* build-msi.ps1 in the same build directory. It
    refuses rather than falling back to the build tree if that has not happened.

    ── Requirements ────────────────────────────────────────────────────────────

    Inno Setup **6.3 or newer** (ISCC.exe) — app.iss.in uses the
    `x64compatible` architecture spelling, which 6.2 does not know. GitHub's
    windows-latest runners do not ship Inno at all:
    `choco install innosetup -y --no-progress`, which gives 6.4.

    ── Not verified end to end ─────────────────────────────────────────────────

    Everything up to the ISCC call is exercised: app-info.json is read, the
    staged tree is found and checked, and the template substitutes with no
    token left behind. The compile itself has never run — there was no Inno
    Setup on the machine this was written on. Its first real run is CI.

.PARAMETER BuildDir
    The configured CMake build directory (e.g. ./build).

.PARAMETER OutFile
    Optional. Copy the produced .exe here under the release naming convention
    <Binary>-<version>-windows-x86_64.exe.
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
$info        = Get-Content -Raw $infoPath | ConvertFrom-Json
$binary      = $info.binary
$version     = $info.version
$displayName = $info.displayName
$upgradeGuid = $info.upgradeGuid

if (-not $upgradeGuid) {
    throw ("app-info.json carries no upgradeGuid. The .exe shares the .msi's " +
           'upgrade identity on purpose — see the note in app.iss.in — so it ' +
           'cannot be built without one. Regenerate with a current ' +
           'cmake/MauvelyAppInfo.cmake.')
}

# ── 1. find ISCC ────────────────────────────────────────────────────────────

$iscc = Get-Command ISCC.exe -ErrorAction SilentlyContinue
if (-not $iscc) {
    foreach ($guess in @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe")) {
        if (Test-Path $guess) { $iscc = Get-Item $guess; break }
    }
}
if (-not $iscc) {
    throw ('ISCC.exe (Inno Setup 6) was not found. Install it with ' +
           '`choco install innosetup -y --no-progress`, or put it on PATH.')
}

# ── 2. the staged tree the .msi was built from ──────────────────────────────

$stage = Get-ChildItem -Path (Join-Path $BuildDir '_CPack_Packages') -Directory -Recurse `
             -ErrorAction SilentlyContinue |
         Where-Object { Test-Path (Join-Path $_.FullName "bin\$binary.exe") } |
         Select-Object -First 1
if (-not $stage) {
    throw ("No CPack staging directory holding bin\$binary.exe was found under " +
           "$BuildDir\_CPack_Packages. Run packaging/windows/build-msi.ps1 first: " +
           'this packages the tree that one produces and verifies, so that the ' +
           '.exe and the .msi cannot ship different payloads.')
}
Write-Host "Staged install tree: $($stage.FullName)"

# The same runtime check build-msi.ps1 makes. Cheap, and it is the difference
# between shipping a broken installer and failing a build.
$bin = Join-Path $stage.FullName 'bin'
@('vcruntime140.dll', 'msvcp140.dll', 'Qt6Core.dll') | ForEach-Object {
    if (-not (Test-Path (Join-Path $bin $_))) {
        throw "$_ is missing from the staged tree; the .exe would install an app that cannot start."
    }
}

# ── 3. fill the template ────────────────────────────────────────────────────

$template = Join-Path $PSScriptRoot 'app.iss.in'
if (-not (Test-Path $template)) { throw "Missing $template." }

$outDir   = (Resolve-Path $BuildDir).Path
$baseName = "$binary-$version-windows-x86_64"
# The same .ico the executable and the .msi already use, named in app-info.json
# by mauvely_app_info(ICON ...). Not a second copy under packaging/ — the app's
# mark lives in its own resources, and one of the two would go stale.
$icon = ''
if ($info.PSObject.Properties.Name -contains 'windowsIcon' -and $info.windowsIcon) {
    if (Test-Path $info.windowsIcon) {
        $icon = (Resolve-Path $info.windowsIcon).Path
    } else {
        Write-Host "::warning::windowsIcon $($info.windowsIcon) does not exist; using Inno's default."
    }
}

$tokens = @{
    'DISPLAY_NAME'    = $displayName
    'BINARY'          = $binary
    'VERSION'         = $version
    'PUBLISHER'       = 'Mauvely'
    'UPGRADE_GUID'    = $upgradeGuid
    'STAGE_DIR'       = $stage.FullName
    'OUTPUT_DIR'      = $outDir
    'OUTPUT_BASENAME' = $baseName
    'ICON'            = $icon
}

$script = Get-Content -Raw $template
foreach ($k in $tokens.Keys) { $script = $script.Replace("@$k@", $tokens[$k]) }
if ($script -match '@[A-Z_]+@') {
    throw "Unsubstituted token left in the Inno script: $($Matches[0])"
}
# No SetupIconFile line at all rather than an empty one, which ISCC rejects.
if (-not $icon) { $script = $script -replace '(?m)^SetupIconFile=.*\r?\n', '' }

$issPath = Join-Path $BuildDir 'app.iss'
Set-Content -LiteralPath $issPath -Value $script -Encoding UTF8
Write-Host "Inno script: $issPath"

# ── 4. compile ──────────────────────────────────────────────────────────────

& $iscc.Source '/Qp' $issPath
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE." }

$exe = Join-Path $outDir "$baseName.exe"
if (-not (Test-Path $exe)) { throw "ISCC reported success but produced no $exe." }
Write-Host ("Built {0} ({1:N1} MB)" -f $exe, ((Get-Item $exe).Length / 1MB))

if ($OutFile) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutFile) | Out-Null
    Copy-Item -LiteralPath $exe -Destination $OutFile -Force
    Write-Host "Copied to $OutFile"
}
