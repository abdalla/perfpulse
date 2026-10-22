# Downloads the signed PawnIO modules PerfPulse uses for CPU temperature and
# package power, and places them in <repo>\modules (next to the exe in a
# distribution). The PawnIO driver itself is installed separately from
# https://pawnio.eu; it refuses to run modules without a valid signature.
param(
    [string]$Version = '0.2.11',
    [string]$Destination = (Join-Path $PSScriptRoot '..\modules')
)
$ErrorActionPreference = 'Stop'

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("pawnio-modules-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $zip = Join-Path $work 'modules.zip'
    $tag = $Version.Replace('.', '_')
    $url = "https://github.com/namazso/PawnIO.Modules/releases/download/$Version/release_$tag.zip"
    Write-Host "Downloading $url"
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $work 'extracted')

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    $wanted = 'IntelMSR.bin', 'AMDFamily17.bin'
    foreach ($name in $wanted) {
        $file = Get-ChildItem -Path (Join-Path $work 'extracted') -Recurse -Filter $name | Select-Object -First 1
        if (-not $file) { throw "$name not found in the release archive" }
        Copy-Item $file.FullName (Join-Path $Destination $name) -Force
        Write-Host "  $name  $((Get-FileHash $file.FullName -Algorithm SHA256).Hash)"
    }
    Write-Host "Modules copied to $((Resolve-Path $Destination).ProviderPath)"
}
finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
