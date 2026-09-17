<#
.SYNOPSIS
    Builds every saver in Release|x64 and collects the .scr files into artifacts\.
.PARAMETER Configuration
    Build configuration (default Release).
.PARAMETER Clean
    Rebuild from scratch.
#>
param(
    [string]$Configuration = "Release",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $msbuild = & $vswhere -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe | Select-Object -First 1
    }
}
if (-not $msbuild -or -not (Test-Path $msbuild)) { throw "MSBuild.exe not found." }

$target = if ($Clean) { "Rebuild" } else { "Build" }
& $msbuild (Join-Path $root "RetroSavers.slnx") "/t:$target" "/p:Configuration=$Configuration" "/p:Platform=x64" /m /nologo /v:m
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }

$binDir = Join-Path $root "bin\x64-$Configuration"
$artifacts = Join-Path $root "artifacts"
New-Item -ItemType Directory -Force $artifacts | Out-Null

$savers = Get-ChildItem (Join-Path $root "Savers") -Directory | ForEach-Object { $_.Name }
foreach ($name in $savers) {
    $scr = Join-Path $binDir "$name.scr"
    if (Test-Path $scr) {
        Copy-Item $scr (Join-Path $artifacts "$name.scr") -Force
        Write-Host "  $name.scr  ->  artifacts\"
    } else {
        Write-Warning "$name.scr not found in $binDir"
    }
}
$host_ = Join-Path $binDir "PreviewHost.exe"
if (Test-Path $host_) { Copy-Item $host_ (Join-Path $artifacts "PreviewHost.exe") -Force }
Write-Host "Done. Artifacts in $artifacts"
