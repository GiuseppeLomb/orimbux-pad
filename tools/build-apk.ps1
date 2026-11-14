# Orimbux Pad — compila o APK de debug e copia-o para apk\
# Uso (PowerShell, na pasta do projeto):  .\tools\build-apk.ps1

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 1) Dependências que não vão para o git (SDL3)
& (Join-Path $PSScriptRoot 'fetch-deps.ps1')

# 2) Java: usa o JAVA_HOME se existir; senão o java do PATH
if (-not $env:JAVA_HOME) {
    $java = (Get-Command java -ErrorAction SilentlyContinue).Source
    if (-not $java) { throw "Não encontrei o Java (JDK 17 ou mais recente). Instala-o ou define JAVA_HOME." }
    $env:JAVA_HOME = Split-Path -Parent (Split-Path -Parent $java)
}

# 3) Compila (chama o wrapper do Gradle diretamente: funciona mesmo com "&" no nome da pasta)
$jar = Join-Path $root 'gradle\wrapper\gradle-wrapper.jar'
& "$env:JAVA_HOME\bin\java.exe" "-Dorg.gradle.appname=gradlew" -classpath $jar org.gradle.wrapper.GradleWrapperMain assembleDebug
if ($LASTEXITCODE -ne 0) { throw "A compilação falhou." }

# 4) Copia o APK para um sítio fácil de encontrar
$out = Join-Path $root 'apk'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$apk = Join-Path $root 'app\build\outputs\apk\debug\app-debug.apk'
$dest = Join-Path $out 'OrimbuxPad-debug.apk'
Copy-Item $apk $dest -Force
Write-Host "APK pronto: $dest"
