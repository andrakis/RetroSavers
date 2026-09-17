<#
.SYNOPSIS
    Copies the built .scr files to %LOCALAPPDATA%\RetroSavers and optionally selects one
    as the active screen saver (no admin required).
.PARAMETER Saver
    Name of the saver to activate (e.g. Starfield). Omit to only copy files.
.PARAMETER System
    Also copy into C:\Windows\System32 so the savers appear in the Screen Saver Settings
    dropdown next to Bubbles/Ribbons. Requires an elevated prompt.
.EXAMPLE
    .\build\install.ps1 -Saver Pipes3D
#>
param(
    [string]$Saver,
    [switch]$System
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "artifacts"
if (-not (Test-Path $source) -or -not (Get-ChildItem $source -Filter *.scr)) {
    Write-Host "No artifacts found; running publish.ps1 first..."
    & (Join-Path $PSScriptRoot "publish.ps1")
}

$dest = Join-Path $env:LOCALAPPDATA "RetroSavers"
New-Item -ItemType Directory -Force $dest | Out-Null
Get-ChildItem $source -Filter *.scr | ForEach-Object {
    Copy-Item $_.FullName (Join-Path $dest $_.Name) -Force
    Write-Host "  $($_.Name)  ->  $dest"
}

if ($System) {
    $sys = Join-Path $env:SystemRoot "System32"
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { throw "-System needs an elevated PowerShell." }
    Get-ChildItem $source -Filter *.scr | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $sys $_.Name) -Force
        Write-Host "  $($_.Name)  ->  $sys"
    }
}

if ($Saver) {
    $path = Join-Path $dest "$Saver.scr"
    if (-not (Test-Path $path)) { throw "$path not found." }
    # Sets HKCU\Control Panel\Desktop\SCRNSAVE.EXE and opens the saver's settings dialog.
    Start-Process rundll32.exe -ArgumentList "desk.cpl,InstallScreenSaver `"$path`""
    Write-Host "Installed $Saver as the active screen saver."
}
