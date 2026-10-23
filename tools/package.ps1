# Builds a release and packs the portable zip that goes on GitHub Releases.
#
#   .\tools\package.ps1              # build + tests, then zip
#   .\tools\package.ps1 -SkipBuild   # zip the existing build\release\PerfPulse.exe
#
# Output: build\PerfPulse-v<version>-win64.zip with PerfPulse.exe, the PawnIO
# modules (plus their LGPL-2.1 license), LICENSE.txt and README.txt.
param(
    [switch]$SkipBuild,
    [string]$PawnIOModulesVersion = '0.2.11'
)
$ErrorActionPreference = 'Stop'

$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$cmakeLists = Get-Content (Join-Path $root 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\(PerfPulse\s+VERSION\s+([0-9.]+)') { throw 'Version not found in CMakeLists.txt' }
$version = $Matches[1]

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Config release
}
$exe = Join-Path $root 'build\release\PerfPulse.exe'
if (-not (Test-Path $exe)) { throw "$exe not found; run without -SkipBuild" }

$modules = Join-Path $root 'modules'
if (-not (Test-Path (Join-Path $modules 'IntelMSR.bin')) -or -not (Test-Path (Join-Path $modules 'AMDFamily17.bin'))) {
    & (Join-Path $PSScriptRoot 'fetch_pawnio_modules.ps1') -Version $PawnIOModulesVersion
}

$stage = Join-Path $root 'build\package\PerfPulse'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $stage 'modules') | Out-Null

Copy-Item $exe $stage
Copy-Item (Join-Path $modules '*.bin') (Join-Path $stage 'modules')
Copy-Item (Join-Path $root 'LICENSE') (Join-Path $stage 'LICENSE.txt')
(Get-Content (Join-Path $root 'resources\dist\README.txt') -Raw).Replace('{VERSION}', "v$version") |
    Set-Content (Join-Path $stage 'README.txt') -Encoding utf8
# LGPL-2.1 requires shipping the license text with the module binaries.
$licenseUrl = "https://raw.githubusercontent.com/namazso/PawnIO.Modules/$PawnIOModulesVersion/COPYING"
Invoke-WebRequest -Uri $licenseUrl -OutFile (Join-Path $stage 'modules\LICENSE-PawnIO.Modules.txt') -UseBasicParsing

$zip = Join-Path $root "build\PerfPulse-v$version-win64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
# Compress-Archive and ZipFile on Windows PowerShell 5.1 write '\' separators
# into the zip, which other unzip tools treat as part of the file name.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    $base = Split-Path $stage -Parent
    foreach ($file in Get-ChildItem $stage -Recurse -File) {
        $entry = $file.FullName.Substring($base.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, 'Optimal') | Out-Null
    }
}
finally {
    $archive.Dispose()
}

Write-Host "`nPackaged $zip" -ForegroundColor Green
Write-Host "SHA256 $((Get-FileHash $zip -Algorithm SHA256).Hash)"
