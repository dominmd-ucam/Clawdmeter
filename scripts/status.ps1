# status.ps1 - Muestra si todo esta bien: programa en marcha, arranque automatico, Bluetooth, sesion de Claude y log.
#
# Uso:  powershell -ExecutionPolicy Bypass -File "$env:USERPROFILE\Clawdmeter\scripts\status.ps1"

. (Join-Path $PSScriptRoot "common.ps1")

Write-Host ("Usuario : " + $env:USERNAME)
Write-Host ("Proyecto: " + $script:RepoRoot)

Write-Step "Programa"
$procs = @(Get-TrayProcesses)
if ($procs.Count -gt 0) { Write-Ok ("En marcha (" + $procs.Count + " proceso/s)") } else { Write-Bad "No esta en marcha. Arrancalo con scripts\start.ps1" }

$run = $null
try { $run = (Get-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -ErrorAction Stop).Clawdmeter } catch { }
if ($run) { Write-Ok "Arranque automatico activado" } else { Write-Note "Arranque automatico NO activado (se activa con scripts\install.ps1)" }

Write-Step "Bluetooth"
$dev = @()
try { $dev = @(Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -eq "Clawdmeter" }) } catch { }
if ($dev.Count -gt 0) { Write-Ok "'Clawdmeter' esta emparejado en este PC" } else { Write-Bad "'Clawdmeter' NO esta emparejado. Windows > Bluetooth > Agregar dispositivo" }

Write-Step "Sesion de Claude Code"
if (Test-Path $script:CredFile) {
    $age = [int]((Get-Date) - (Get-Item $script:CredFile).LastWriteTime).TotalHours
    Write-Ok ("Encontrada (modificada hace " + $age + " h). Si el log da 401, abre 'claude' y haz /login.")
} else {
    Write-Bad ("No existe " + $script:CredFile + ". Abre 'claude' e inicia sesion.")
}

Write-Step "Configuracion"
if (Test-Path $script:ConfigFile) {
    Write-Host ("   " + $script:ConfigFile)
    Get-Content $script:ConfigFile | ForEach-Object { Write-Host ("     " + $_) }
} else {
    Write-Note "Aun no existe (se crea con scripts\install.ps1)"
}

Write-Step "Ultimas lineas del log"
if (Test-Path $script:LogFile) {
    Get-Content $script:LogFile -Tail 12 | ForEach-Object {
        if ($_.Length -gt 200) { Write-Host ("   " + $_.Substring(0, 200) + " ...") } else { Write-Host ("   " + $_) }
    }
    Write-Host ""
    Write-Host ("   Log completo: " + $script:LogFile)
} else {
    Write-Note "No hay log todavia (el programa no ha arrancado nunca en este PC)"
}
