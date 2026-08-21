<#
.SYNOPSIS
    Builds the Microsoft Store package (.msix) for a Mauvely desktop app.

.DESCRIPTION
    Three steps, and the first is the one worth understanding:

      1. `cmake --install` into a staging tree. NOT a copy of build/bin. The
         install rule in cmake/MauvelyPackaging.cmake runs windeployqt over the
         *staged* tree, which is what actually puts Qt6Widgets.dll and the
         platform plugin beside the executable. Compose shipped 0.4.0 with an
         .msi built from the build tree and the app died before main() with a
         loader dialog. The same trap is waiting here.
      2. Generate the three tile images the manifest names, from the same 256px
         brand icon the Linux packaging uses.
      3. MakeAppx.exe pack.

    Nothing here names an app: identity comes from build/app-info.json, written
    by cmake/MauvelyAppInfo.cmake. Play's copy of this script hardcoded
    "MauvelyCompose" and was never run by CI, so nobody found out.

    The package is left UNSIGNED on purpose — a Store submission is re-signed by
    Microsoft. That is also why it cannot be sideloaded as-is: `Add-AppxPackage`
    will refuse it, which is expected. To smoke-test locally, sign it with a
    self-signed certificate you have trusted.

.PARAMETER BuildDir
    The configured CMake build directory (e.g. ./build).

.PARAMETER IdentityName
    Package identity reserved in Partner Center, e.g. Mauvely.MauvelySnap.

.PARAMETER Publisher
    The publisher string from Partner Center, e.g. "CN=A1B2C3D4-...". An
    identity, not a display name: "CN=Mauvely" is rejected at upload.

.PARAMETER PublisherDisplayName
    The publisher display name on the Partner Center account.

.PARAMETER OutFile
    Where to write the .msix. Defaults to <BuildDir>/<Binary>-<version>-windows-x86_64.msix.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$IdentityName,
    [Parameter(Mandatory = $true)][string]$Publisher,
    [Parameter(Mandatory = $true)][string]$PublisherDisplayName,
    [string]$OutFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$stage    = Join-Path $BuildDir 'msix-stage'
$assets   = Join-Path $stage 'Assets'

# ── 0. who are we? ──────────────────────────────────────────────────────────

$infoPath = Join-Path $BuildDir 'app-info.json'
if (-not (Test-Path $infoPath)) {
    throw "$infoPath not found — configure the project first (cmake -S . -B $BuildDir)."
}
$info = Get-Content -Raw $infoPath | ConvertFrom-Json
$binary      = $info.binary
$displayName = $info.displayName
$description = $info.displayName
$version     = $info.version

if (-not ($version -match '^\d+\.\d+\.\d+$')) {
    throw "Version must be x.y.z (got '$version'). The Store owns the fourth field."
}
# The Store rejects a non-zero revision: it reserves the fourth field for the
# rebuilds it does itself when it re-signs or re-targets a package.
$packageVersion = "$version.0"

if (-not $OutFile) {
    $OutFile = Join-Path $BuildDir "$binary-$version-windows-x86_64.msix"
}

# ── 1. stage the install tree ───────────────────────────────────────────────

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Write-Host "Staging the install tree into $stage"
& cmake --install $BuildDir --config Release --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install failed with exit code $LASTEXITCODE." }

$bin = Join-Path $stage 'bin'
$exe = Join-Path $bin "$binary.exe"
if (-not (Test-Path $exe)) {
    throw "$binary.exe is not at $exe. The manifest's Executable path is " +
          "bin\$binary.exe, so a change to CMAKE_INSTALL_BINDIR has to be " +
          "mirrored in packaging/windows/msix/AppxManifest.xml.in."
}

# A package that installs cleanly and cannot start is the one failure no compile
# step catches. The base set is what every Qt Widgets app needs; the rest is
# derived from what this app actually links, so an app that gains WebEngine gets
# the stricter check without anyone editing this list.
$required = @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'platforms\qwindows.dll')
$hasWebEngine = Test-Path (Join-Path $bin 'Qt6WebEngineCore.dll')
if ($hasWebEngine) {
    $required += @('Qt6WebEngineWidgets.dll', 'QtWebEngineProcess.exe')
}
$missing = @($required | Where-Object { -not (Test-Path (Join-Path $bin $_)) })
if ($hasWebEngine -and
    -not (Get-ChildItem -Path $bin -Recurse -Filter '*.pak' -ErrorAction SilentlyContinue)) {
    # The exact set of .pak files varies by Qt version, so require one rather
    # than a name.
    $missing += 'qtwebengine_resources.pak (resources\)'
}
if ($missing.Count -gt 0) {
    Write-Host "::error::Refusing to package — the .msix installs an app that cannot start."
    $missing | ForEach-Object { Write-Host "  missing: $_" }
    Get-ChildItem -Recurse $bin | Select-Object -ExpandProperty FullName
    throw 'Qt runtime incomplete in the staged install tree.'
}
Write-Host ("Qt runtime staged: {0} files." -f (Get-ChildItem -Recurse -File $bin).Count)

# ── 2. tile images ──────────────────────────────────────────────────────────

# Scaled from the 256px brand icon rather than checked in at three more sizes,
# which could silently fall out of step with the Linux icons.
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force -Path $assets | Out-Null

$sourceIcon = Join-Path $repoRoot 'packaging\linux\icons\256.png'
if (-not (Test-Path $sourceIcon)) { throw "Brand icon not found at $sourceIcon." }

function Write-Tile {
    param([string]$Path, [int]$Size)

    $src = [System.Drawing.Image]::FromFile($sourceIcon)
    try {
        $bmp = New-Object System.Drawing.Bitmap $Size, $Size
        try {
            $g = [System.Drawing.Graphics]::FromImage($bmp)
            try {
                # Thin strokes and transparent corners: anything less than
                # high-quality bicubic on a downscale turns them to mush at 44px,
                # which is the size shown in the taskbar.
                $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $g.Clear([System.Drawing.Color]::Transparent)
                $g.DrawImage($src, 0, 0, $Size, $Size)
            } finally { $g.Dispose() }
            $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $bmp.Dispose() }
    } finally { $src.Dispose() }

    Write-Host ("  {0} ({1}x{1})" -f (Split-Path -Leaf $Path), $Size)
}

Write-Host 'Generating tile images:'
Write-Tile -Path (Join-Path $assets 'Square44x44Logo.png')   -Size 44
Write-Tile -Path (Join-Path $assets 'Square150x150Logo.png') -Size 150
Write-Tile -Path (Join-Path $assets 'StoreLogo.png')         -Size 50

# ── 3. manifest + pack ──────────────────────────────────────────────────────

$template = Join-Path $PSScriptRoot 'msix\AppxManifest.xml.in'
if (-not (Test-Path $template)) { throw "Manifest template not found at $template." }

# XML-escape before substitution: the description is prose and the publisher
# display name is whatever the account is called. An unescaped & in either
# produces a manifest MakeAppx rejects with a parse error pointing at a line
# number in generated output.
function ConvertTo-XmlText([string]$value) {
    return $value.Replace('&', '&amp;').Replace('<', '&lt;').Replace('>', '&gt;').Replace('"', '&quot;')
}

$manifest = Get-Content -Raw $template
$tokens = @{
    '@IDENTITY_NAME@'          = $IdentityName
    '@PUBLISHER@'              = $Publisher
    '@PUBLISHER_DISPLAY_NAME@' = $PublisherDisplayName
    '@VERSION@'                = $packageVersion
    '@BINARY@'                 = $binary
    '@DISPLAY_NAME@'           = $displayName
    '@DESCRIPTION@'            = $description
}
foreach ($token in $tokens.Keys) {
    $manifest = $manifest.Replace($token, (ConvertTo-XmlText $tokens[$token]))
}
if ($manifest -match '@[A-Z_]+@') {
    throw "A manifest token was left unsubstituted: $($Matches[0])."
}

$manifestPath = Join-Path $stage 'AppxManifest.xml'
# No BOM. MakeAppx reads the manifest as XML and a UTF-8 BOM ahead of the
# declaration makes it fail with a malformed-document error.
[System.IO.File]::WriteAllText($manifestPath, $manifest, (New-Object System.Text.UTF8Encoding($false)))
Write-Host "Manifest written: $IdentityName $packageVersion"

# MakeAppx ships in the Windows SDK, whose path carries an SDK version, so it is
# not on PATH and cannot be hardcoded. Newest first: an older SDK on the same
# runner packs an older manifest schema.
$makeAppx = Get-ChildItem -Path @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "$env:ProgramFiles\Windows Kits\10\bin"
    ) -Filter 'makeappx.exe' -Recurse -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' } |
    Sort-Object -Property FullName -Descending |
    Select-Object -First 1

if (-not $makeAppx) {
    throw 'makeappx.exe was not found. Install the Windows 10/11 SDK (the ' +
          'windows-latest runner image ships it; a self-hosted runner may not).'
}
Write-Host "Using $($makeAppx.FullName)"

$outDir = Split-Path -Parent $OutFile
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force -Path $outDir | Out-Null }

& $makeAppx.FullName pack /d $stage /p $OutFile /o
if ($LASTEXITCODE -ne 0) { throw "makeappx pack failed with exit code $LASTEXITCODE." }

$size = (Get-Item $OutFile).Length
Write-Host ("Packed {0} ({1:N1} MB)" -f $OutFile, ($size / 1MB))
