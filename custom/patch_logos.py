# Sustituye los iconos genericos por tus PNG.
# Uso: python patch_logos.py <raiz_del_repo> <carpeta_con_png>
# Busca en la carpeta (y subcarpetas) PNG cuyo nombre contenga:
#   claude | github | devops o azure | shopify | minipc, mini o server
# Los que no encuentre se quedan como estan.
import sys, re, pathlib
from PIL import Image

root = pathlib.Path(sys.argv[1]); folder = pathlib.Path(sys.argv[2])
hdr_path = root / "firmware" / "src" / "icons_services.h"
s = hdr_path.read_text(encoding="utf-8")
assert "icon_sv_claude_data" in s, "Aplica antes patch_all.py"

SLOTS = [("claude", ["claude"]), ("github", ["github"]), ("devops", ["devops", "azure"]),
         ("shopify", ["shopify"]), ("minipc", ["minipc", "mini", "server", "pc"])]
SKIP = {".venv", ".pio", ".git", "node_modules"}
pngs = [p for p in folder.rglob("*.png") if not (set(p.parts) & SKIP)]

def prepare(im, name):
    im = im.convert("RGBA")
    px = im.load(); w, h = im.size
    # 1) sin transparencia: quita el color de fondo de la esquina
    if px[0, 0][3] > 250:
        bg = px[0, 0][:3]
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) < 60:
                    px[x, y] = (r, g, b, 0)
        print(f"  {name}: sin transparencia, fondo {bg} quitado")
    # 2) icono oscuro -> blanco (la placa tiene fondo oscuro)
    lum = [0.299*r + 0.587*g + 0.114*b for r, g, b, a in im.getdata() if a > 128]
    if lum and sum(lum) / len(lum) < 70:
        for y in range(h):
            for x in range(w):
                r, g, b, a = px[x, y]
                px[x, y] = (255, 255, 255, a)
        print(f"  {name}: icono oscuro, pasado a blanco")
    box = im.getbbox()
    return im.crop(box) if box else im

def to565a8(im):
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

done = []
for name, keys in SLOTS:
    cands = sorted(p for p in pngs if any(k in p.stem.lower() for k in keys))
    exact = [p for p in cands if p.stem.lower() in keys]   # prefiere el nombre exacto (claude.png)
    cands = exact or cands
    if not cands:
        print(f"- {name}: no encuentro PNG, se queda el generico")
        continue
    f = cands[0]
    if len(cands) > 1:
        print(f"- {name}: varios candidatos {[c.name for c in cands]}, uso {f.name}")
    src = Image.open(f)
    print(f"- {name}: {f.name} ({src.width}x{src.height})" + ("  AVISO: muy pequeno, se vera borroso" if max(src.size) < 28 else ""))
    src = prepare(src, name)
    big = Image.new("RGBA", (240, 240), (0, 0, 0, 0))
    src.thumbnail((216, 216), Image.LANCZOS)
    if src.width < 216 and src.height < 216:   # amplia los pequenos
        k = min(216 / src.width, 216 / src.height)
        src = src.resize((max(1, int(src.width * k)), max(1, int(src.height * k))), Image.LANCZOS)
    big.alpha_composite(src, ((240 - src.width) // 2, (240 - src.height) // 2))
    small = big.resize((30, 30), Image.LANCZOS)
    sym = f"icon_sv_{name}_data"
    pat = re.compile(r"static const uint8_t " + sym + r"\[\d+\] = \{.*?\n\};\n", re.S)
    assert len(pat.findall(s)) == 1, f"no encuentro {sym}"
    s = pat.sub(lambda m: carray(sym, to565a8(small)), s, count=1)
    done.append(name)

hdr_path.write_text(s, encoding="utf-8", newline="\n")
print("Iconos sustituidos:", ", ".join(done) if done else "ninguno")
