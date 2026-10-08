# uninstall.ps1 - Cierra el programa y quita el arranque automatico. No borra la carpeta ni tu configuracion.
#
# Uso:  powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\uninstall.ps1"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Step "Cerrando el programa"
$n = Stop-Tray
Write-Ok ("Copias cerradas: " + $n)

Write-Step "Quitando el arranque automatico"
Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "Clawdmeter" -ErrorAction SilentlyContinue
Write-Ok "Hecho"

Write-Host ""
Write-Host "Para quitar tambien el emparejamiento: Windows > Bluetooth y dispositivos > Clawdmeter > Quitar dispositivo."
Write-Host ("Configuracion y log (por si quieres borrarlos): " + $script:DataDir)
