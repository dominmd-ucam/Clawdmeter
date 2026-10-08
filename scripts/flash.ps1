# flash.ps1 - Compila y sube el firmware a la placa por USB-C.
# Solo hace falta cuando cambia el firmware (carpeta firmware/). Para usar la placa a diario NO hace falta.
#
# Uso:
#   powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\flash.ps1"
#   ... flash.ps1 -Port COM5          (si no detecta el puerto solo)
#
# La primera vez tarda ~15 minutos (descarga herramientas). Las siguientes, pocos minutos.

param(
    [string]$Port  = "",
    [string]$Board = "waveshare_amoled_216"
)

. (Join-Path $PSScriptRoot "common.ps1")

Write-Step "1/3 Python y PlatformIO"
$py = Get-SystemPython
if (-not $py) { Write-Bad "No encuentro Python 3. Instalalo y vuelve a intentarlo."; exit 1 }
Write-Ok $py.Text

$pioVersion = ""
try { $pioVersion = (& $py.Exe (@($py.Args) + @("-m", "platformio", "--version")) 2>&1 | Out-String) } catch { }
if ($pioVersion -notmatch "PlatformIO") {
    Write-Note "PlatformIO no esta instalado. Lo instalo (solo esta vez)..."
    & $py.Exe (@($py.Args) + @("-m", "pip", "install", "--user", "platformio"))
    if ($LASTEXITCODE -ne 0) { Write-Bad "No pude instalar PlatformIO"; exit 1 }
}
Write-Ok "PlatformIO listo"

Write-Step "2/3 Buscando la placa"
if (-not $Port) {
    $found = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -and ($_.Name -match "\(COM\d+\)") -and $_.PNPDeviceID -and ($_.PNPDeviceID -match "VID_303A") })
    foreach ($d in $found) {
        if ($d.Name -match "\((COM\d+)\)") { $Port = $Matches[1]; break }
    }
}
if (-not $Port) {
    Write-Bad "No encuentro la placa por USB."
    Write-Host "     1. Conecta la placa con un cable USB-C que lleve datos (no solo carga)."
    Write-Host "     2. Si sigue sin salir: desenchufala, manten pulsado BOOT y enchufala sin soltar."
    Write-Host "     3. Si ya sabes el puerto:  scripts\flash.ps1 -Port COM5"
    exit 1
}
Write-Ok ("Placa en " + $Port)

Write-Step ("3/3 Compilando y subiendo (" + $Board + ")")
Push-Location $script:RepoRoot
try {
    $pioArgs = @("-m", "platformio", "run", "-d", "firmware", "-e", $Board, "-t", "upload", "--upload-port", $Port)
    & $py.Exe (@($py.Args) + $pioArgs)
    $code = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($code -ne 0) {
    Write-Bad ("Fallo la subida (codigo " + $code + "). Si pone 'Connecting...': BOOT pulsado al enchufar y repite.")
    exit $code
}
Write-Host ""
Write-Host "Firmware subido. La placa se reinicia sola." -ForegroundColor Green
Write-Host "Si queda en 'Waiting', reiniciala y reinicia el programa:  scripts\start.ps1"
