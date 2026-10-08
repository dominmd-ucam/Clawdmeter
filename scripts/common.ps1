# common.ps1 - funciones compartidas por los scripts de esta carpeta.
# No se ejecuta solo: lo cargan install.ps1, start.ps1, stop.ps1, status.ps1, flash.ps1 y uninstall.ps1.
# Nada aqui usa rutas fijas: todo se calcula desde la carpeta del repo y el usuario actual.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:RepoRoot   = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$script:VenvDir    = Join-Path $script:RepoRoot ".venv"
$script:VenvPython = Join-Path $script:VenvDir "Scripts\python.exe"
$script:TrayScript = Join-Path $script:RepoRoot "daemon\tray_windows.py"
$script:DataDir    = Join-Path $env:LOCALAPPDATA "Clawdmeter"
$script:LogFile    = Join-Path $script:DataDir "daemon.log"
$script:ConfigFile = Join-Path $script:DataDir "config"

if ($env:CLAUDE_CONFIG_DIR) {
    $script:CredFile = Join-Path $env:CLAUDE_CONFIG_DIR ".credentials.json"
} else {
    $script:CredFile = Join-Path $env:USERPROFILE ".claude\.credentials.json"
}

function Write-Step([string]$Message) {
    Write-Host ""
    Write-Host "== $Message" -ForegroundColor Cyan
}
function Write-Ok([string]$Message)   { Write-Host "   [OK] $Message" -ForegroundColor Green }
function Write-Note([string]$Message) { Write-Host "   [!!] $Message" -ForegroundColor Yellow }
function Write-Bad([string]$Message)  { Write-Host "   [XX] $Message" -ForegroundColor Red }

# Devuelve un hashtable con Exe, Args, Major, Minor, Text; o $null si no hay Python 3.
function Get-SystemPython {
    $options = @(
        @{ Exe = "python"; Args = @() },
        @{ Exe = "py";     Args = @("-3") }
    )
    foreach ($o in $options) {
        if (-not (Get-Command $o.Exe -ErrorAction SilentlyContinue)) { continue }
        $callArgs = @($o.Args) + @("--version")
        $text = ""
        try { $text = (& $o.Exe $callArgs 2>&1 | Out-String) } catch { continue }
        if ($text -match "Python (\d+)\.(\d+)\.(\d+)") {
            return @{
                Exe   = $o.Exe
                Args  = @($o.Args)
                Major = [int]$Matches[1]
                Minor = [int]$Matches[2]
                Text  = $text.Trim()
            }
        }
    }
    return $null
}

# pythonw.exe del Python base del .venv (sin ventana de consola). Igual que hace install-windows.ps1.
function Get-TrayPythonw {
    if (-not (Test-Path $script:VenvPython)) { return $null }
    $base = (& $script:VenvPython -c "import sys; print(sys.base_exec_prefix)" | Out-String).Trim()
    $pw = Join-Path $base "pythonw.exe"
    if (Test-Path $pw) { return $pw }
    return $null
}

# Procesos que estan ejecutando tray_windows.py (solo los de Clawdmeter, no otros pythonw).
function Get-TrayProcesses {
    Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and ($_.CommandLine -like "*tray_windows.py*") }
}

function Stop-Tray {
    $procs = @(Get-TrayProcesses)
    foreach ($p in $procs) {
        try { Stop-Process -Id $p.ProcessId -Force -ErrorAction Stop } catch { }
    }
    return $procs.Count
}

function Start-Tray {
    $pw = Get-TrayPythonw
    if (-not $pw) { throw "No encuentro el entorno .venv. Ejecuta primero: scripts\install.ps1" }
    Start-Process -FilePath $pw -ArgumentList ('"' + $script:TrayScript + '"') -WorkingDirectory $script:RepoRoot
}

# Crea %LOCALAPPDATA%\Clawdmeter\config con valores por defecto si no existe. Devuelve $true si lo ha creado.
function Initialize-Config {
    if (-not (Test-Path $script:DataDir)) {
        New-Item -ItemType Directory -Force -Path $script:DataDir | Out-Null
    }
    if (Test-Path $script:ConfigFile) { return $false }
    $lines = @(
        "# Clawdmeter - configuracion del programa de Windows.",
        "# Se relee en cada consulta (cada ~60 s): no hace falta reiniciar.",
        "clock = 24",
        "chime = off",
        "# IP del mini PC para el ping del panel Servicios (si no se pone, la casilla queda en gris):",
        "# minipc = 192.168.1.50",
        "# Varias cuentas de Claude (opcional): config_dirs = ~/.claude, ~/.claude-work"
    )
    Set-Content -Path $script:ConfigFile -Value $lines -Encoding ASCII
    return $true
}
