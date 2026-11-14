# Orimbux Pad — instala o APK no telemóvel por ADB sem fios (Wi-Fi).
# Uso:  .\tools\install-apk.ps1 192.168.1.50:41234
#       (IP:porta que aparece em Definições → Opções de programador → Depuração sem fios)
# O emparelhamento (adb pair) faz-se uma vez antes: ver docs\02-RAIO-X.md.

param([Parameter(Mandatory = $true)][string]$Endereco)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Procura o adb: SDK local (local.properties) ou PATH.
$adb = (Get-Command adb -ErrorAction SilentlyContinue).Source
if (-not $adb) {
    $lp = Join-Path $root 'local.properties'
    if (Test-Path $lp) {
        $sdk = ((Get-Content $lp | Where-Object { $_ -match '^sdk.dir=' }) -replace '^sdk.dir=', '') -replace '\\\\', '\' -replace '\\:', ':'
        $cand = Join-Path $sdk 'platform-tools\adb.exe'
        if (Test-Path $cand) { $adb = $cand }
    }
}
if (-not $adb) { throw "Não encontrei o adb. Instala as Android SDK Platform-Tools." }

$apk = Join-Path $root 'apk\OrimbuxPad-debug.apk'
if (-not (Test-Path $apk)) { throw "Não há APK. Corre primeiro .\tools\build-apk.ps1" }

& $adb connect $Endereco
& $adb -s $Endereco install -r $apk
if ($LASTEXITCODE -eq 0) { Write-Host "Instalado. Abre a app 'Orimbux Pad' no telemóvel." }
