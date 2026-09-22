param(
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$control = Join-Path $PSScriptRoot "paladin-control.ps1"
$runtime = Join-Path $root "debug\runtime"

New-Item -ItemType Directory -Force -Path $runtime | Out-Null

if ([string]::IsNullOrWhiteSpace($OutFile)) {
    $OutFile = Join-Path $runtime "latest-frame.png"
} elseif (-not [System.IO.Path]::IsPathRooted($OutFile)) {
    $OutFile = Join-Path $root $OutFile
}

& $control screenshot $OutFile
if ($LASTEXITCODE -ne 0) {
    throw "Paladin isolated screenshot failed."
}

Write-Output $OutFile
