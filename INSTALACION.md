# Instalacion paso a paso (Windows)

Todo se instala en tu carpeta de usuario (`$env:USERPROFILE`), asi que funciona igual en cualquier PC.

## Requisitos
- Windows 10/11 con Bluetooth
- Python 3.10 o superior (marca "Add python.exe to PATH" al instalarlo)
- Git
- Claude Code instalado y con sesion iniciada (`claude`)

## 1. Clonar
```powershell
cd $env:USERPROFILE
git clone https://github.com/dominmd-ucam/Clawdmeter.git Clawdmeter
cd $env:USERPROFILE\Clawdmeter
```

## 2. Instalar
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1
```
Crea el entorno `.venv`, la configuracion y el arranque automatico.

## 3. Emparejar la placa
1. Enciende la placa (pantalla "Waiting").
2. Ajustes de Windows > Bluetooth > Anadir dispositivo > "Clawdmeter".
3. La pantalla pasara a "Listening" y luego "Connected".

## Uso diario
```powershell
cd $env:USERPROFILE\Clawdmeter
.\scripts\status.ps1   # estado, log, emparejado
.\scripts\start.ps1    # arrancar / reiniciar
.\scripts\stop.ps1     # parar
```

## Flashear el firmware (solo si cambias el firmware)
Con la placa por USB:
```powershell
.\scripts\flash.ps1            # autodetecta el puerto
.\scripts\flash.ps1 -Port COM3
```

## Cambiar la placa a otro PC
1. En la placa: mantén PWR ~3 s y suelta (borra los emparejamientos).
2. En el PC antiguo: elimina "Clawdmeter" de Bluetooth (y `.\scripts\stop.ps1`).
3. En el PC nuevo: pasos 1-3.

## Actualizar
```powershell
git pull
.\scripts\start.ps1
```

## Problemas frecuentes
| Sintoma | Solucion |
|---|---|
| "Waiting" | Sin Bluetooth: `status.ps1`, comprueba emparejado; si falla, borra el dispositivo en Windows, PWR largo en la placa y empareja de nuevo |
| "Listening / No data" | Conectado pero sin datos: mira el log (`status.ps1`) |
| 401 / token caducado | Abre Claude Code una vez para renovar la sesion |
| "Connection lost" | `.\scripts\start.ps1` |

## Configuracion
Fichero `%LOCALAPPDATA%\Clawdmeter\config`: `clock` (12/24), `chime` (on/off), `minipc` (IP para ping), `config_dirs` (varias cuentas Claude).
Log: `%LOCALAPPDATA%\Clawdmeter\daemon.log`.

## Desinstalar
```powershell
.\scripts\uninstall.ps1
```

> Recomendado: repositorio **privado** (contiene tus ajustes y coordenadas).
