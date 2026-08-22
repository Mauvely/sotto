<#
.SYNOPSIS
    The one entry point for producing Windows artifacts.

.DESCRIPTION
    scripts/package.ps1 [-Format msi|msix|all] [-BuildDir DIR] [-Out DIR]

    Every Mauvely app has this script at this path with these flags, so "how do
    I build a package for X" has one answer across the whole suite. It refuses
    formats the app does not target, read from build/app-info.json rather than
    hardcoded here.

    Linux formats live in scripts/package.sh. There is no cross-building.

    The MSI and the MSIX are NOT interchangeable builds of the same tree. The
    MSI keeps the self-updater; the MSIX must not have it, because an MSIX
    cannot replace its own package — a Store build that tried to self-update
    would download successfully, fail to apply, and leave a permanent
    "restart to update" that never finishes. So configure with the app's
    *_STORE_BUILD option OFF for the MSI and ON for the MSIX, which is what
    release.yml does in two passes. This script packages whatever is currently
    configured and says which it found.

.PARAMETER Format
    msi, msix or all. Default: all.

.PARAMETER BuildDir
    The configured CMake build directory. Default: build.

.PARAMETER Out
    Optional directory to copy finished artifacts into.

.PARAMETER IdentityName
    MSIX only. Partner Center package identity. Defaults to $env:MSSTORE_IDENTITY_NAME.

.PARAMETER Publisher
    MSIX only. The CN=<guid> publisher identity. Defaults to $env:MSSTORE_PUBLISHER.

.PARAMETER PublisherDisplayName
    MSIX only. Defaults to $env:MSSTORE_PUBLISHER_DISPLAY.
#>
[CmdletBinding()]
param(
    [ValidateSet('msi', 'msix', 'all')][string]$Format = 'all',
    [string]$BuildDir = 'build',
    [string]$Out,
    [string]$IdentityName        = $env:MSSTORE_IDENTITY_NAME,
    [string]$Publisher           = $env:MSSTORE_PUBLISHER,
    [string]$PublisherDisplayName = $env:MSSTORE_PUBLISHER_DISPLAY
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $IsWindows -and $PSVersionTable.PSVersion.Major -ge 6) {
    throw "Windows packaging must run on Windows. Use scripts/package.sh for AppImage and Flatpak."
}

$infoPath = Join-Path $BuildDir 'app-info.json'
if (-not (Test-Path $infoPath)) {
    throw "$infoPath not found. Configure first, e.g. cmake -S . -B $BuildDir -DCMAKE_BUILD_TYPE=Release"
}
$info      = Get-Content -Raw $infoPath | ConvertFrom-Json
$binary    = $info.binary
$version   = $info.version
$supported = $info.formats.windows

if (-not $supported) {
    Write-Host 'This app does not ship for Windows — see PLATFORMS in CMakeLists.txt.'
    Write-Host 'Linux artifacts are built by scripts/package.sh on Linux.'
    exit 0
}

function Test-Wanted([string]$name) {
    if ($Format -ne 'all' -and $Format -ne $name) { return $false }
    if ($supported -notcontains $name) {
        # `all` means "everything this app targets", so an app that drops a
        # format from app-info.json stops building it without editing this file.
        if ($Format -eq $name) {
            throw "This app does not target $name (targets: $($supported -join ', '))."
        }
        return $false
    }
    return $true
}

$built = @()

if (Test-Wanted 'msi') {
    Write-Host '==> MSI'
    & (Join-Path $repoRoot 'packaging\windows\build-msi.ps1') -BuildDir $BuildDir
    $built += (Join-Path $BuildDir "$binary-$version-windows-x86_64.msi")
}

if (Test-Wanted 'msix') {
    # Named individually rather than "one of these is missing": the first
    # release attempt always trips on exactly one of the three, and a message
    # that says which saves a round trip through Partner Center.
    $unset = @()
    if (-not $IdentityName)         { $unset += 'IdentityName / MSSTORE_IDENTITY_NAME' }
    if (-not $Publisher)            { $unset += 'Publisher / MSSTORE_PUBLISHER' }
    if (-not $PublisherDisplayName) { $unset += 'PublisherDisplayName / MSSTORE_PUBLISHER_DISPLAY' }
    if ($unset.Count -gt 0) {
        throw "MSIX needs the Partner Center identity. Not set: $($unset -join ', ')."
    }
    if ($Publisher -notlike 'CN=*') {
        # The most common first-release failure, and the upload error does not
        # say where to look.
        throw "Publisher must be the Partner Center publisher identity and start with 'CN=' " +
              "(got '$Publisher'). It is a CN=<guid>, not a company name."
    }

    Write-Host '==> MSIX'
    & (Join-Path $repoRoot 'packaging\windows\build-msix.ps1') `
        -BuildDir $BuildDir `
        -IdentityName $IdentityName `
        -Publisher $Publisher `
        -PublisherDisplayName $PublisherDisplayName
    $built += (Join-Path $BuildDir "$binary-$version-windows-x86_64.msix")
}

if ($built.Count -eq 0) {
    throw "Nothing was built — check -Format against the app's targets ($($supported -join ', '))."
}

if ($Out) {
    New-Item -ItemType Directory -Force -Path $Out | Out-Null
    $built | ForEach-Object { Copy-Item -LiteralPath $_ -Destination $Out -Force }
}

Write-Host ''
Write-Host 'Built:'
$built | ForEach-Object {
    Write-Host ("  {0}  ({1:N1} MB)" -f $_, ((Get-Item $_).Length / 1MB))
}
