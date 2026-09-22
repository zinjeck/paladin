param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$ErrorActionPreference = "Stop"

$source = Join-Path $PSScriptRoot "PaladinDesktopControl.cpp"
$exe = Join-Path $PSScriptRoot "PaladinDesktopControl.exe"
$builder = Join-Path $PSScriptRoot "build-paladin-desktop-control.bat"

if (
    -not (Test-Path $exe) -or
    (Get-Item $source).LastWriteTimeUtc -gt (Get-Item $exe).LastWriteTimeUtc
) {
    & $builder
    if ($LASTEXITCODE -ne 0) {
        throw "Paladin desktop controller build failed."
    }
}

& $exe @Arguments
exit $LASTEXITCODE
