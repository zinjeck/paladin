param(
    [switch]$Build,
    [switch]$Restart
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$control = Join-Path $PSScriptRoot "paladin-control.ps1"
$buildDir = Join-Path $root "out\build\x64-Debug"
$vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if ($Build) {
    & $control stop | Out-Null

    if (-not (Test-Path (Join-Path $buildDir "build.ninja"))) {
        $cmd = 'call "' + $vcvars + '" >nul && cmake -S "' + $root + '" -B "' + $buildDir + '" -G Ninja -DCMAKE_BUILD_TYPE=Debug'
        & cmd.exe /d /s /c $cmd
        if ($LASTEXITCODE -ne 0) {
            throw "Paladin configure failed."
        }
    }

    $cmd = 'call "' + $vcvars + '" >nul && cmake --build "' + $buildDir + '" --target Paladin'
    & cmd.exe /d /s /c $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "Paladin build failed."
    }
}

$args = @("launch")
if ($Restart) {
    $args += "restart"
}

& $control @args
exit $LASTEXITCODE
