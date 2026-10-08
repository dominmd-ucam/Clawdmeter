# Personalizaciones (custom)

Esta carpeta guarda lo que se anadio sobre el proyecto original (Clawdmeter Plus):

- Tiempo y hora de Murcia en la pantalla (coordenadas en `WX_URL` dentro de `daemon/claude_usage_daemon_windows.py`).
- Panel "Servicios": Claude, GitHub, Azure DevOps, Shopify y MiniPC (ping), con su logo. Solo colores (verde/rojo/gris), sin parpadeo.
- Porcentaje de bateria junto al icono.

## Cambiar los iconos
1. Sustituye los PNG de `custom/icons/` (claude, github, devops, shopify, pc), idealmente 24x24.
2. Ejecuta desde la raiz del repo:
   ```powershell
   python custom\patch_logos.py . custom\icons
   ```
   Regenera `firmware/src/icons_services.h`.
3. Reflashea: `.\scripts\flash.ps1`

`patch_all.py` ya esta aplicado al codigo; se conserva solo como referencia.

## MiniPC
Pon su IP en `%LOCALAPPDATA%\Clawdmeter\config`:
```
minipc = 192.168.1.50
```
