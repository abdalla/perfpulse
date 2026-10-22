# Builds PerfPulse with MSVC + Ninja.
#
#   .\tools\build.ps1                 # Release build + tests
#   .\tools\build.ps1 -Config debug   # Debug build
#
# CMake and Ninja do not cope well with UNC paths (\\wsl.localhost\...), so
# when the repository lives on a network/WSL share it is mapped to a drive
# letter for the duration of the build.
param(
    [ValidateSet('debug', 'release')][string]$Config = 'release',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio Build Tools with the C++ workload were not found.' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'

$source = (Resolve-Path (Join-Path $PSScriptRoot '..')).ProviderPath
$mapped = $null
if ($source.StartsWith('\\')) {
    $letter = @('P', 'Q', 'R', 'S', 'T') | Where-Object { -not (Test-Path "${_}:\") } | Select-Object -First 1
    $share = ($source -split '\\')[0..3] -join '\'
    $rest = $source.Substring($share.Length)
    net use "${letter}:" $share /persistent:no | Out-Null
    $mapped = "${letter}:"
    $source = "${letter}:$rest"
}

try {
    $steps = "cmake --preset $Config && cmake --build --preset $Config"
    if (-not $SkipTests) { $steps += " && ctest --preset $Config" }
    cmd /c "`"$vcvars`" >nul && cd /d `"$source`" && $steps"
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
    $exe = Join-Path $source "build\$Config\PerfPulse.exe"
    Write-Host "`nBuilt $exe" -ForegroundColor Green
}
finally {
    if ($mapped) { net use $mapped /delete /y | Out-Null }
}
