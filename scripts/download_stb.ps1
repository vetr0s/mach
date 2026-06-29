# Download the stb single-header libraries (Windows parity of download_stb.sh).

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$stbDir = Join-Path $repoRoot "third_party\stb"
$stbRepo = "https://raw.githubusercontent.com/nothings/stb/master"

if (-not (Test-Path $stbDir)) {
    New-Item -ItemType Directory -Path $stbDir -Force | Out-Null
}

Write-Host "Downloading stb headers..."
foreach ($h in @("stb_image.h", "stb_truetype.h", "stb_rect_pack.h")) {
    Invoke-WebRequest -Uri "$stbRepo/$h" -OutFile (Join-Path $stbDir $h)
}

Write-Host "stb headers ready at $stbDir" -ForegroundColor Green
