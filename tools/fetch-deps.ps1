# Orimbux Pad — descarrega as dependências que não vão para o git.
# Uso (PowerShell, na pasta do projeto):  .\tools\fetch-deps.ps1
#
# - SDL3 (zlib): camada do comando DualSense (driver HIDAPI PS5).
#   Fica em third_party/SDL3 e é compilado pelo CMake da app.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$tp = Join-Path $root 'third_party'
New-Item -ItemType Directory -Force -Path $tp | Out-Null

$sdlVersion = '3.4.20'
$sdlDir = Join-Path $tp 'SDL3'
$stamp = Join-Path $sdlDir '.version'

if ((Test-Path $stamp) -and ((Get-Content $stamp -Raw).Trim() -eq $sdlVersion)) {
    Write-Host "SDL3 $sdlVersion já está em third_party/SDL3"
    exit 0
}

$zip = Join-Path $tp "SDL3-$sdlVersion.zip"
$url = "https://github.com/libsdl-org/SDL/releases/download/release-$sdlVersion/SDL3-$sdlVersion.zip"
Write-Host "A descarregar $url"
curl.exe -sSL -f -o $zip $url
if ($LASTEXITCODE -ne 0) { throw "Falhou o download do SDL3" }

$tmp = Join-Path $tp '_sdl_tmp'
if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
Expand-Archive -Path $zip -DestinationPath $tmp -Force
if (Test-Path $sdlDir) { Remove-Item -Recurse -Force $sdlDir }
Move-Item (Join-Path $tmp "SDL3-$sdlVersion") $sdlDir
Remove-Item -Recurse -Force $tmp
Remove-Item -Force $zip
Set-Content -Path $stamp -Value $sdlVersion
Write-Host "SDL3 $sdlVersion pronto em third_party/SDL3"
