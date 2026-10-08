# stop.ps1 - Cierra el programa de Clawdmeter. No toca el arranque automatico (para eso: uninstall.ps1).
#
# Uso:  powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\stop.ps1"

. (Join-Path $PSScriptRoot "common.ps1")

$n = Stop-Tray
if ($n -gt 0) { Write-Ok ("Cerradas " + $n + " copia(s) de Clawdmeter") } else { Write-Note "No habia ninguna copia en marcha" }
