# install.ps1 - Instala Clawdmeter en este PC (Windows). Se puede ejecutar varias veces sin problema.
#
# Uso, desde cualquier sitio:
#   powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\install.ps1"
#
# No toca el codigo del proyecto: prepara el .venv, la configuracion y llama al instalador original
# (install-windows.ps1), que registra el arranque automatico y lanza el programa de la bandeja.

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host "Clawdmeter - instalacion en Windows" -ForegroundColor Cyan
Write-Host ("Usuario : " + $env:USERNAME)
Write-Host ("Proyecto: " + $script:RepoRoot)

# ---------------------------------------------------------------- 1. Python
Write-Step "1/5 Comprobando Python"
$py = Get-SystemPython
if (-not $py) {
    Write-Bad "No encuentro Python 3."
    Write-Host "     Instalalo desde https://www.python.org/downloads/ (marca 'Add python.exe to PATH'),"
    Write-Host "     cierra y abre PowerShell, y vuelve a ejecutar este script."
    exit 1
}
if (($py.Major -lt 3) -or (($py.Major -eq 3) -and ($py.Minor -lt 10))) {
    Write-Bad ("Hace falta Python 3.10 o superior. Tienes: " + $py.Text)
    exit 1
}
Write-Ok $py.Text

# ---------------------------------------------------------------- 2. venv
Write-Step "2/5 Entorno virtual (.venv)"
if (Test-Path $script:VenvPython) {
    $works = $false
    try {
        $t = (& $script:VenvPython "--version" 2>&1 | Out-String)
        if ($t -match "Python 3") { $works = $true }
    } catch { }
    if (-not $works) {
        Write-Note ".venv no funciona en este PC (copiado de otro equipo?). Lo recreo."
        Stop-Tray | Out-Null
        Remove-Item -Recurse -Force $script:VenvDir
    }
}
if (-not (Test-Path $script:VenvPython)) {
    $venvArgs = @($py.Args) + @("-m", "venv", $script:VenvDir)
    & $py.Exe $venvArgs
    if ($LASTEXITCODE -ne 0) { Write-Bad "No pude crear el .venv"; exit 1 }
}
Write-Ok ".venv listo"

# ---------------------------------------------------------------- 3. Bluetooth y Claude Code
Write-Step "3/5 Comprobando Bluetooth y la sesion de Claude Code"
$bt = @()
try { $bt = @(Get-PnpDevice -Class Bluetooth -Status OK -ErrorAction SilentlyContinue) } catch { }
if ($bt.Count -gt 0) {
    Write-Ok "Bluetooth detectado"
} else {
    Write-Note "No detecto Bluetooth. Sin adaptador (integrado o USB) no se podra conectar con la placa."
}
if (Test-Path $script:CredFile) {
    Write-Ok "Sesion de Claude Code encontrada"
} else {
    Write-Note ("No encuentro " + $script:CredFile)
    Write-Host "     Abre una terminal, ejecuta 'claude' e inicia sesion (/login). Sin eso la placa no recibira datos."
}

# ---------------------------------------------------------------- 4. Configuracion
Write-Step "4/5 Configuracion"
if (Initialize-Config) {
    Write-Ok ("Creada " + $script:ConfigFile + " (reloj de 24 h)")
} else {
    Write-Ok ("Ya existe " + $script:ConfigFile + " (no la toco)")
}

# ---------------------------------------------------------------- 5. Instalador original
Write-Step "5/5 Instalando el programa de la bandeja y el arranque automatico"
Stop-Tray | Out-Null
$installer = Join-Path $script:RepoRoot "install-windows.ps1"
& powershell -NoProfile -ExecutionPolicy Bypass -File $installer
if ($LASTEXITCODE -ne 0) { Write-Bad "Fallo install-windows.ps1 (codigo $LASTEXITCODE)"; exit 1 }

Write-Host ""
Write-Host "Instalacion terminada." -ForegroundColor Green
Write-Host ""
Write-Host "Siguiente: emparejar la placa por Bluetooth"
Write-Host "  1. Enciende la placa: debe poner 'Waiting'."
Write-Host "  2. Windows > Configuracion > Bluetooth y dispositivos > Agregar dispositivo > Bluetooth > Clawdmeter."
Write-Host "  3. En ~1 minuto la pantalla pasa a mostrar tu uso de Claude."
Write-Host ""
Write-Host "Comprobar estado:"
Write-Host ('  powershell -ExecutionPolicy Bypass -File "' + (Join-Path $script:RepoRoot "scripts\status.ps1") + '"')
