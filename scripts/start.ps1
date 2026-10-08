# start.ps1 - Arranca (o reinicia) el programa de Clawdmeter de la bandeja.
# Si ya estaba en marcha, lo cierra primero: asi nunca quedan dos copias peleando por el Bluetooth.
#
# Uso:  powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\start.ps1"

. (Join-Path $PSScriptRoot "common.ps1")

$n = Stop-Tray
if ($n -gt 0) {
    Write-Note ("Cerradas " + $n + " copia(s) anterior(es)")
    Start-Sleep -Seconds 2
}
Start-Tray
Write-Ok "Clawdmeter arrancado (icono en la bandeja del sistema)"
Write-Host "     Si la placa sigue en 'Waiting' o 'Listening' tras 30 s, reiniciala (apagar y encender)."
