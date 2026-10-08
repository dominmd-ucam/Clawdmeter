# Parche Clawdmeter para Domingo: servicios (iconos), Murcia, bateria %, estado de servicios en el daemon.
# Uso: python patch_all.py <raiz_del_repo>      (usar el python de .venv: tiene Pillow)
import sys, math, pathlib
from PIL import Image, ImageDraw

root = pathlib.Path(sys.argv[1])
FW = root / "firmware" / "src"
DM = root / "daemon" / "claude_usage_daemon_windows.py"

def rd(p): return p.read_text(encoding="utf-8")
def wr(p, s): p.write_text(s, encoding="utf-8", newline="\n")
def rep(s, old, new, name, count=1):
    assert s.count(old) == count, f"[{name}] ancla no encontrada (o repetida): {old[:60]!r}"
    return s.replace(old, new)

# ===================== 1. ICONOS (genericos, 30x30 RGB565A8) =====================
S = 8
def newimg():
    im = Image.new("RGBA", (30 * S, 30 * S), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im), 30 * S

TERRA = (217, 119, 87, 255); WHITE = (255, 255, 255, 255); LIGHT = (225, 228, 234, 255)
BLUE = (66, 148, 232, 255); GREEN = (112, 176, 70, 255); GREY = (110, 122, 140, 255)
LED = (90, 220, 120, 255); CLEAR = (0, 0, 0, 0)

def ic_claude():                     # burbuja de chat con chispa
    im, d, W = newimg()
    d.rounded_rectangle([W*0.10, W*0.12, W*0.90, W*0.74], radius=W*0.16, fill=TERRA)
    d.polygon([(W*0.26, W*0.70), (W*0.24, W*0.92), (W*0.46, W*0.72)], fill=TERRA)
    cx, cy, r = W*0.5, W*0.43, W*0.21
    pts = []
    for k in range(8):
        a = k * math.pi / 4
        rr = r if k % 2 == 0 else r * 0.28
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=WHITE)
    return im

def ic_github():                     # grafo de ramas
    im, d, W = newimg()
    w = int(W * 0.07)
    d.line([(W*0.30, W*0.22), (W*0.30, W*0.78)], fill=LIGHT, width=w)
    d.line([(W*0.30, W*0.64), (W*0.44, W*0.52), (W*0.72, W*0.40)], fill=LIGHT, width=w, joint="curve")
    for (x, y) in [(0.30, 0.20), (0.30, 0.80), (0.72, 0.42)]:
        r = W * 0.11
        d.ellipse([W*x - r, W*y - r, W*x + r, W*y + r], fill=LIGHT)
        r2 = W * 0.04
        d.ellipse([W*x - r2, W*y - r2, W*x + r2, W*y + r2], fill=CLEAR)
    return im

def ic_devops():                     # engranaje
    im, d, W = newimg()
    cx = cy = W * 0.5
    for k in range(8):
        a = k * math.pi / 4
        ca, sa = math.cos(a), math.sin(a)
        hw, r0, r1 = W * 0.075, W * 0.30, W * 0.45
        p = [(cx + ca*r0 - sa*hw, cy + sa*r0 + ca*hw), (cx + ca*r1 - sa*hw, cy + sa*r1 + ca*hw),
             (cx + ca*r1 + sa*hw, cy + sa*r1 - ca*hw), (cx + ca*r0 + sa*hw, cy + sa*r0 - ca*hw)]
        d.polygon(p, fill=BLUE)
    r = W * 0.34
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=BLUE)
    r = W * 0.14
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=CLEAR)
    return im

def ic_shopify():                    # bolsa de compra
    im, d, W = newimg()
    d.arc([W*0.34, W*0.08, W*0.66, W*0.46], 180, 360, fill=GREEN, width=int(W*0.07))
    d.rounded_rectangle([W*0.20, W*0.28, W*0.80, W*0.88], radius=W*0.09, fill=GREEN)
    d.arc([W*0.36, W*0.40, W*0.64, W*0.66], 20, 160, fill=WHITE, width=int(W*0.06))
    return im

def ic_minipc():                     # caja con LED
    im, d, W = newimg()
    d.rounded_rectangle([W*0.10, W*0.34, W*0.90, W*0.74], radius=W*0.09, fill=GREY)
    d.rounded_rectangle([W*0.18, W*0.44, W*0.52, W*0.50], radius=W*0.02, fill=(60, 68, 82, 255))
    d.rounded_rectangle([W*0.18, W*0.56, W*0.52, W*0.62], radius=W*0.02, fill=(60, 68, 82, 255))
    r = W * 0.055
    d.ellipse([W*0.72 - r, W*0.53 - r, W*0.72 + r, W*0.53 + r], fill=LED)
    d.rectangle([W*0.30, W*0.74, W*0.70, W*0.80], fill=(80, 90, 106, 255))
    return im

def to565a8(im):
    im = im.resize((30, 30), Image.LANCZOS)
    px = im.load(); col = bytearray(); al = bytearray()
    for y in range(30):
        for x in range(30):
            r, g, b, a = px[x, y]
            c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            col.append(c & 0xFF); col.append((c >> 8) & 0xFF); al.append(a)
    return bytes(col) + bytes(al)

def carray(sym, data):
    rows = [", ".join("0x%02X" % data[i + j] for j in range(min(16, len(data) - i)))
            for i in range(0, len(data), 16)]
    return f"static const uint8_t {sym}[{len(data)}] = {{\n    " + ",\n    ".join(rows) + "\n};\n"

ICONS = [("icon_sv_claude_data", ic_claude), ("icon_sv_github_data", ic_github),
         ("icon_sv_devops_data", ic_devops), ("icon_sv_shopify_data", ic_shopify),
         ("icon_sv_minipc_data", ic_minipc)]
hdr = "// Iconos genericos de servicios (30x30 RGB565A8). Generado por patch_all.py\n#pragma once\n#include <stdint.h>\n\n"
for sym, fn in ICONS:
    hdr += carray(sym, to565a8(fn())) + "\n"
wr(FW / "icons_services.h", hdr)
if len(sys.argv) > 2:                # vista previa opcional
    sheet = Image.new("RGBA", (5 * 160, 160), (31, 31, 30, 255))
    for i, (_, fn) in enumerate(ICONS):
        big = fn().resize((30, 30), Image.LANCZOS).resize((150, 150), Image.NEAREST)
        sheet.alpha_composite(big, (i * 160 + 5, 5))
    sheet.convert("RGB").save(sys.argv[2])

# ===================== 2. FIRMWARE =====================
# --- data.h
p = FW / "data.h"; s = rd(p)
if "agent_unknown" not in s:
    s = rep(s, "    bool agent_busy[5];", "    bool agent_unknown[5];   // slot state unknown ('?' in ag): shown gray\n    bool agent_busy[5];", "data.h")
    wr(p, s)

# --- main.cpp
p = FW / "main.cpp"; s = rd(p)
if "agent_unknown" not in s:
    s = rep(s, "    const char* bz = doc[\"bz\"] | \"\";",
            "    for (int i = 0; i < 5; i++) out->agent_unknown[i] = (out->agents_present && ag[i] == '?');\n    const char* bz = doc[\"bz\"] | \"\";", "main.cpp")
    wr(p, s)

# --- ui.cpp
p = FW / "ui.cpp"; s = rd(p)
if "icons_services.h" not in s:
    s = rep(s, '#include "icons_status.h"\n', '#include "icons_status.h"\n#include "icons_services.h"\n', "include")
    s = rep(s, "    icon_ag_general_data, icon_ag_etsy_data, icon_ag_upwork_data,\n    icon_ag_appdev_data, icon_ag_heirpaws_data\n};",
            "    icon_sv_claude_data, icon_sv_github_data, icon_sv_devops_data,\n    icon_sv_shopify_data, icon_sv_minipc_data\n};\n"
            "static const char* const SV_NAMES[5] = {\"Claude\", \"GitHub\", \"DevOps\", \"Shopify\", \"MiniPC\"};", "icons")
    # Murcia, sin bandera
    s = rep(s, "    lv_obj_set_pos(img_flag, 0, 3);\n", "    lv_obj_set_pos(img_flag, 0, 3);\n    lv_obj_add_flag(img_flag, LV_OBJ_FLAG_HIDDEN);   // Murcia: sin bandera\n", "flag")
    s = rep(s, 'lv_label_set_text(wlbl, "London");', 'lv_label_set_text(wlbl, "Murcia");', "murcia")
    s = rep(s, "    lv_obj_set_pos(wlbl, 40, 1);", "    lv_obj_set_pos(wlbl, 0, 1);", "murcia-pos")
    # panel servicios
    s = rep(s, 'lv_label_set_text(albl, "Agents");', 'lv_label_set_text(albl, "Servicios");', "albl")
    s = rep(s, "lv_obj_set_size(hl, hl_w, 88);", "lv_obj_set_size(hl, hl_w, 100);", "hl")
    s = rep(s, "        agent_dots[i] = dot;\n",
            "        agent_dots[i] = dot;\n\n"
            "        lv_obj_t* nl = lv_label_create(apanel);\n"
            "        lv_label_set_text(nl, SV_NAMES[i]);\n"
            "        lv_obj_set_style_text_font(nl, &font_styrene_16, 0);\n"
            "        lv_obj_set_style_text_color(nl, COL_DIM, 0);\n"
            "        lv_obj_set_style_text_align(nl, LV_TEXT_ALIGN_CENTER, 0);\n"
            "        lv_obj_set_width(nl, inner_w / 5);\n"
            "        lv_obj_set_pos(nl, center_x - inner_w / 10, 102);\n"
            "        lv_obj_add_flag(nl, LV_OBJ_FLAG_EVENT_BUBBLE);\n", "names")
    # estado gris si '?'
    s = rep(s, "                                      data->agents[i] ? COL_GREEN : COL_RED, 0);",
            "                                      data->agent_unknown[i] ? COL_DIM\n                                      : (data->agents[i] ? COL_GREEN : COL_RED), 0);", "unknown")
    # bateria %
    s = rep(s, "static lv_obj_t* battery_img;\n", "static lv_obj_t* battery_img;\nstatic lv_obj_t* lbl_batt_pct = nullptr;\n", "battvar")
    s = rep(s, "    lv_obj_set_pos(battery_img, L.scr_w - 48 - L.margin, L.title_y);\n",
            "    lv_obj_set_pos(battery_img, L.scr_w - 48 - L.margin, L.title_y);\n\n"
            "    lbl_batt_pct = lv_label_create(scr);\n"
            "    lv_label_set_text(lbl_batt_pct, \"\");\n"
            "    lv_obj_set_style_text_font(lbl_batt_pct, &font_styrene_20, 0);\n"
            "    lv_obj_set_style_text_color(lbl_batt_pct, COL_DIM, 0);\n"
            "    lv_obj_set_style_text_align(lbl_batt_pct, LV_TEXT_ALIGN_RIGHT, 0);\n"
            "    lv_obj_set_width(lbl_batt_pct, 70);\n"
            "    lv_obj_align_to(lbl_batt_pct, battery_img, LV_ALIGN_OUT_LEFT_MID, -2, 0);\n", "battlabel")
    s = rep(s, "    if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);\n    else                                  lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN);\n",
            "    if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(battery_img, LV_OBJ_FLAG_HIDDEN);\n    else                                  lv_obj_clear_flag(battery_img, LV_OBJ_FLAG_HIDDEN);\n"
            "    if (lbl_batt_pct) {\n        if (current_screen == SCREEN_SPLASH) lv_obj_add_flag(lbl_batt_pct, LV_OBJ_FLAG_HIDDEN);\n        else                                  lv_obj_clear_flag(lbl_batt_pct, LV_OBJ_FLAG_HIDDEN);\n    }\n", "battvis")
    s = rep(s, "    lv_image_set_src(battery_img, &battery_dscs[idx]);\n    apply_battery_visibility();",
            "    lv_image_set_src(battery_img, &battery_dscs[idx]);\n"
            "    if (lbl_batt_pct) {\n        if (percent < 0) lv_label_set_text(lbl_batt_pct, \"\");\n        else             lv_label_set_text_fmt(lbl_batt_pct, \"%d%%\", percent);\n    }\n"
            "    apply_battery_visibility();", "battupd")
    wr(p, s)

# ===================== 3. DAEMON =====================
s = rd(DM)
if "add_service_fields" not in s:
    block = '''
# ---- Estado de servicios (panel "Servicios" de la placa) ----
# ag: 5 caracteres, '1' operativo, '0' caido, '?' desconocido (gris). bz: '1' degradado (naranja).
# Orden: Claude, GitHub, DevOps, Shopify, MiniPC
SERVICE_URLS = [
    "https://status.claude.com/api/v2/status.json",
    "https://www.githubstatus.com/api/v2/status.json",
    "https://status.dev.azure.com/_apis/status/health?api-version=6.0-preview.1",
    "https://www.shopifystatus.com/api/v2/status.json",
]
_SV_TTL = 180
_sv_cache = {"ts": 0.0, "ag": "?????", "bz": "00000"}
_sv_last_log = {}


def read_config_value(key: str) -> str:
    try:
        if CONFIG_FILE.exists():
            for line in CONFIG_FILE.read_text().splitlines():
                line = line.split("#", 1)[0].strip()
                if "=" not in line:
                    continue
                k, v = line.split("=", 1)
                if k.strip().lower() == key:
                    return v.strip()
    except OSError:
        pass
    return ""


def _classify(data: dict):
    """Devuelve (ag, bz) para Statuspage (status.indicator) o Azure DevOps (status.health)."""
    st = data.get("status", {})
    ind = str(st.get("indicator", "")).lower()
    if ind:
        if ind == "none":
            return "1", "0"
        if ind in ("minor", "maintenance"):
            return "1", "1"
        return "0", "0"
    health = str(st.get("health", "")).lower()
    if health:
        if health == "healthy":
            return "1", "0"
        if health in ("advisory", "degraded"):
            return "1", "1"
        return "0", "0"
    return "?", "0"


async def _check_status_url(http, name: str, url: str):
    try:
        resp = await http.get(url, headers={"User-Agent": "clawdmeter-status/1.0"})
        res = _classify(resp.json())
    except Exception as e:
        res = ("?", "0")
        if _sv_last_log.get(name) != "err":
            log(f"service {name}: check failed: {e}")
        _sv_last_log[name] = "err"
        return res
    if _sv_last_log.get(name) != res:
        log(f"service {name}: ag={res[0]} bz={res[1]}")
        _sv_last_log[name] = res
    return res


def _ping(host: str) -> bool:
    try:
        r = subprocess.run(["ping", "-n", "1", "-w", "1500", host], capture_output=True, text=True,
                           timeout=6, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        return "TTL=" in r.stdout.upper()
    except (OSError, subprocess.SubprocessError):
        return False


async def add_service_fields(payload: dict) -> None:
    now = time.time()
    if now - _sv_cache["ts"] > _SV_TTL:
        names = ["Claude", "GitHub", "DevOps", "Shopify"]
        async with httpx.AsyncClient(timeout=8.0, follow_redirects=True) as http:
            results = await asyncio.gather(*[_check_status_url(http, n, u) for n, u in zip(names, SERVICE_URLS)])
        host = read_config_value("minipc")
        if host:
            ok = await asyncio.to_thread(_ping, host)
            results.append(("1" if ok else "0", "0"))
            if _sv_last_log.get("MiniPC") != ok:
                log(f"service MiniPC ({host}): {'up' if ok else 'down'}")
                _sv_last_log["MiniPC"] = ok
        else:
            results.append(("?", "0"))
        _sv_cache.update(ts=now, ag="".join(r[0] for r in results), bz="".join(r[1] for r in results))
    payload["ag"] = _sv_cache["ag"]
    payload["bz"] = _sv_cache["bz"]

'''
    s = rep(s, "\n\nasync def poll_api(token: str)", "\n" + block + "\nasync def poll_api(token: str)", "daemon-def")
    call_anchor = "    await add_weather_fields(payload)\n"
    if call_anchor not in s:   # por si el parche del tiempo no esta aplicado
        call_anchor = "    add_clock_fields(payload)   # adds \"t\" + \"tf\" iff the config opts in\n"
    s = rep(s, call_anchor, call_anchor + "    await add_service_fields(payload)\n", "daemon-call")
    wr(DM, s)

print("Parche aplicado: iconos, firmware y daemon")
