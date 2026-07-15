#!/usr/bin/env python3
# Generate colored RGB565A8 icons for the Clawdmeter status page.
# Draws each icon supersampled with PIL, downscales (LANCZOS) for anti-aliasing,
# emits a C header (firmware/src/icons_status.h) AND a preview contact sheet PNG.
import math, os
from PIL import Image, ImageDraw

S = 8  # supersample factor

# ---- palette ----
WHITE=(255,255,255,255)
BLUE_UJ=(1,45,120,255); RED_UJ=(200,16,46,255)
SUN=(246,190,60,255); SUN_CORE=(247,160,40,255)
CLOUD=(205,208,214,255); CLOUD_DK=(150,154,165,255)
RAIN=(74,150,225,255)
SNOW=(228,242,255,255)
BOLT=(255,205,55,255)
FOG=(180,183,190,255)
OCEAN=(46,120,185,255); LAND=(80,178,86,255)
BAG=(217,119,87,255)          # terracotta (etsy)
BRIEF=(160,108,64,255); BRIEF_DK=(120,78,44,255)   # brown briefcase (upwork)
PHONE=(45,58,74,255); SCREEN=(120,185,235,255)     # phone (appdev)
PAW=(150,118,104,255)         # warm brown paw (heirpaws)

def newimg(w,h):
    im = Image.new('RGBA',(w*S,h*S),(0,0,0,0))
    return im, ImageDraw.Draw(im), w*S, h*S

def sun(d,cx,cy,r,core=SUN_CORE,ray=SUN,rays=True):
    if rays:
        for k in range(8):
            a=k*math.pi/4
            x1=cx+math.cos(a)*r*1.35; y1=cy+math.sin(a)*r*1.35
            x2=cx+math.cos(a)*r*2.0;  y2=cy+math.sin(a)*r*2.0
            d.line([(x1,y1),(x2,y2)],fill=ray,width=int(r*0.30))
            d.ellipse([x2-r*0.15,y2-r*0.15,x2+r*0.15,y2+r*0.15],fill=ray)
    d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=SUN)
    d.ellipse([cx-r*0.62,cy-r*0.62,cx+r*0.62,cy+r*0.62],fill=core)

def cloud(d,cx,cy,w,color=CLOUD):
    r1=w*0.28; r2=w*0.36; r3=w*0.30
    base_top=cy+r1*0.15; base_bot=cy+r1*1.0
    d.ellipse([cx-w*0.55,cy-r1,cx-w*0.55+2*r1,cy+r1],fill=color)          # left lobe
    d.ellipse([cx-r2,cy-r2*1.35,cx+r2*0.5,cy-r2*1.35+2*r2],fill=color)     # top-mid big lobe
    d.ellipse([cx+w*0.55-2*r3,cy-r3,cx+w*0.55,cy+r3],fill=color)           # right lobe
    d.rounded_rectangle([cx-w*0.55,base_top-r1,cx+w*0.55,base_bot],
                        radius=r1*0.9,fill=color)

def emit_flag():
    im,d,W,H = newimg(30,18)
    d.rounded_rectangle([0,0,W-1,H-1],radius=H*0.10,fill=BLUE_UJ)
    d.line([(0,0),(W,H)],fill=WHITE,width=int(H*0.30))
    d.line([(0,H),(W,0)],fill=WHITE,width=int(H*0.30))
    d.line([(0,0),(W,H)],fill=RED_UJ,width=int(H*0.13))
    d.line([(0,H),(W,0)],fill=RED_UJ,width=int(H*0.13))
    wc=int(H*0.36); rc=int(H*0.19)
    d.rectangle([W/2-wc/2,0,W/2+wc/2,H],fill=WHITE)
    d.rectangle([0,H/2-wc/2,W,H/2+wc/2],fill=WHITE)
    d.rectangle([W/2-rc/2,0,W/2+rc/2,H],fill=RED_UJ)
    d.rectangle([0,H/2-rc/2,W,H/2+rc/2],fill=RED_UJ)
    return im.resize((30,18),Image.LANCZOS),"ICON_FLAG_UK","icon_flag_uk_data"

# ---- weather (all 40x40) ----
def W_sun():
    im,d,W,H=newimg(40,40); sun(d,W*0.5,H*0.5,W*0.24); return im
def W_suncloud():
    im,d,W,H=newimg(40,40)
    sun(d,W*0.34,H*0.34,W*0.17)
    cloud(d,W*0.56,H*0.60,W*0.72)
    return im
def W_cloud():
    im,d,W,H=newimg(40,40); cloud(d,W*0.5,H*0.46,W*0.82); return im
def W_fog():
    im,d,W,H=newimg(40,40)
    cloud(d,W*0.5,H*0.34,W*0.80,color=(210,213,219,255))
    for i,yy in enumerate([0.68,0.80,0.92]):
        x0=W*(0.14+0.04*(i%2))
        d.line([(x0,H*yy),(W-x0,H*yy)],fill=FOG,width=int(H*0.045))
    return im
def W_rain():
    im,d,W,H=newimg(40,40)
    cloud(d,W*0.5,H*0.36,W*0.80,color=CLOUD_DK)
    for x in [0.34,0.52,0.70]:
        d.line([(W*x,H*0.66),(W*(x-0.09),H*0.92)],fill=RAIN,width=int(H*0.055))
    return im
def W_snow():
    im,d,W,H=newimg(40,40)
    cx,cy,r=W*0.5,H*0.5,W*0.30
    for k in range(6):
        a=k*math.pi/3
        d.line([(cx,cy),(cx+math.cos(a)*r,cy+math.sin(a)*r)],fill=SNOW,width=int(W*0.05))
        # small branches
        for t in (0.55,0.8):
            bx,by=cx+math.cos(a)*r*t,cy+math.sin(a)*r*t
            for s in (a+math.pi/3,a-math.pi/3):
                d.line([(bx,by),(bx+math.cos(s)*r*0.22,by+math.sin(s)*r*0.22)],fill=SNOW,width=int(W*0.04))
    d.ellipse([cx-r*0.12,cy-r*0.12,cx+r*0.12,cy+r*0.12],fill=SNOW)
    return im
def W_thunder():
    im,d,W,H=newimg(40,40)
    cloud(d,W*0.5,H*0.34,W*0.80,color=CLOUD_DK)
    bolt=[(W*0.52,H*0.55),(W*0.40,H*0.78),(W*0.50,H*0.78),(W*0.40,H*0.98),
          (W*0.66,H*0.68),(W*0.54,H*0.68),(W*0.60,H*0.55)]
    d.polygon(bolt,fill=BOLT)
    return im

def resize40(im): return im.resize((40,40),Image.LANCZOS)

# ---- agent icons (all 30x30) ----
def A_globe():
    im,d,W,H=newimg(30,30)
    cx,cy,r=W*0.5,H*0.5,W*0.42
    d.ellipse([cx-r,cy-r,cx+r,cy+r],fill=OCEAN)
    # land blobs
    d.ellipse([cx-r*0.7,cy-r*0.6,cx-r*0.05,cy+r*0.1],fill=LAND)
    d.ellipse([cx+r*0.05,cy-r*0.15,cx+r*0.72,cy+r*0.7],fill=LAND)
    d.ellipse([cx-r*0.35,cy+r*0.25,cx+r*0.1,cy+r*0.75],fill=LAND)
    # meridians (white arcs)
    d.arc([cx-r,cy-r,cx+r,cy+r],0,360,fill=WHITE,width=int(W*0.045))
    d.line([(cx,cy-r),(cx,cy+r)],fill=(255,255,255,180),width=int(W*0.04))
    d.arc([cx-r*0.5,cy-r,cx+r*0.5,cy+r],0,360,fill=(255,255,255,150),width=int(W*0.035))
    d.line([(cx-r,cy),(cx+r,cy)],fill=(255,255,255,150),width=int(W*0.035))
    return im
def A_bag():
    im,d,W,H=newimg(30,30)
    # handle
    d.arc([W*0.34,H*0.10,W*0.66,H*0.46],180,360,fill=BAG,width=int(W*0.06))
    # body (trapezoid-ish rounded)
    d.rounded_rectangle([W*0.24,H*0.30,W*0.76,H*0.86],radius=W*0.10,fill=BAG)
    d.line([(W*0.24,H*0.44),(W*0.76,H*0.44)],fill=(255,255,255,120),width=int(W*0.03))
    return im
def A_brief():
    im,d,W,H=newimg(30,30)
    d.rounded_rectangle([W*0.40,H*0.15,W*0.60,H*0.37],radius=W*0.05,outline=BRIEF_DK,width=int(W*0.06))  # handle
    d.rounded_rectangle([W*0.14,H*0.30,W*0.86,H*0.84],radius=W*0.09,fill=BRIEF)
    d.rectangle([W*0.14,H*0.50,W*0.86,H*0.57],fill=BRIEF_DK)  # divider
    d.rounded_rectangle([W*0.45,H*0.48,W*0.55,H*0.59],radius=W*0.02,fill=BRIEF_DK)  # clasp
    return im
def A_phone():
    im,d,W,H=newimg(30,30)
    d.rounded_rectangle([W*0.30,H*0.10,W*0.70,H*0.90],radius=W*0.10,fill=PHONE)
    d.rounded_rectangle([W*0.36,H*0.20,W*0.64,H*0.76],radius=W*0.04,fill=SCREEN)
    d.ellipse([W*0.47,H*0.80,W*0.53,H*0.86],fill=(200,205,212,255))
    return im
def A_paw():
    im,d,W,H=newimg(30,30)
    # main pad - large, centered
    d.ellipse([W*0.28,H*0.46,W*0.72,H*0.88],fill=PAW)
    # 4 oval toe beans arced above, taller than wide
    for (tx,ty,rx,ry) in [(0.28,0.34,0.095,0.13),(0.44,0.24,0.105,0.145),
                          (0.58,0.24,0.105,0.145),(0.73,0.34,0.095,0.13)]:
        d.ellipse([W*(tx-rx),H*(ty-ry),W*(tx+rx),H*(ty+ry)],fill=PAW)
    return im

def resize30(im): return im.resize((30,30),Image.LANCZOS)

def to_rgb565a8(im):
    w,h=im.size; px=im.load()
    color=bytearray(); alpha=bytearray()
    for y in range(h):
        for x in range(w):
            r,g,b,a=px[x,y]
            c=((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3)
            color.append(c&0xFF); color.append((c>>8)&0xFF)
            alpha.append(a)
    return bytes(color)+bytes(alpha)

def c_array(sym,W_macro,H_macro,w,h,data):
    out=f"#define {W_macro} {w}\n#define {H_macro} {h}\n"
    out+=f"static const uint8_t {sym}[{len(data)}] = {{\n    "
    rows=[]
    for i in range(0,len(data),16):
        rows.append(", ".join("0x%02X"%data[i+j] for j in range(min(16,len(data)-i))))
    out+=",\n    ".join(rows)+"\n};\n"
    return out

# ---- build all ----
icons=[]  # (base, sym, img)
fimg,fmac,fsym=emit_flag(); icons.append(("ICON_FLAG_UK",fsym,fimg))

weather=[("ICON_WX_SUN","icon_wx_sun_data",resize40(W_sun())),
         ("ICON_WX_SUNCLOUD","icon_wx_suncloud_data",resize40(W_suncloud())),
         ("ICON_WX_CLOUD","icon_wx_cloud_data",resize40(W_cloud())),
         ("ICON_WX_FOG","icon_wx_fog_data",resize40(W_fog())),
         ("ICON_WX_RAIN","icon_wx_rain_data",resize40(W_rain())),
         ("ICON_WX_SNOW","icon_wx_snow_data",resize40(W_snow())),
         ("ICON_WX_THUNDER","icon_wx_thunder_data",resize40(W_thunder()))]
agents=[("ICON_AG_GENERAL","icon_ag_general_data",resize30(A_globe())),
        ("ICON_AG_ETSY","icon_ag_etsy_data",resize30(A_bag())),
        ("ICON_AG_UPWORK","icon_ag_upwork_data",resize30(A_brief())),
        ("ICON_AG_APPDEV","icon_ag_appdev_data",resize30(A_phone())),
        ("ICON_AG_HEIRPAWS","icon_ag_heirpaws_data",resize30(A_paw()))]

allicons=[("ICON_FLAG_UK",fsym,fimg)]+weather+agents

# ---- preview contact sheet (dark bg, 4x zoom, labeled) ----
Z=5; pad=14
cols=allicons
sheet_w=len(cols)*(0)  # compute below
tilew=max(im.size[0] for _,_,im in cols)*Z+pad*2
tileh=max(im.size[1] for _,_,im in cols)*Z+pad*2+18
sheet=Image.new('RGBA',(tilew*len(cols),tileh),(31,31,30,255))
sd=ImageDraw.Draw(sheet)
for i,(base,sym,im) in enumerate(cols):
    big=im.resize((im.size[0]*Z,im.size[1]*Z),Image.NEAREST)
    x=i*tilew+ (tilew-big.size[0])//2
    y=pad
    sheet.alpha_composite(big,(x,y))
    sd.text((i*tilew+6,tileh-16),base.replace("ICON_",""),fill=(200,200,200,255))
sheet.convert('RGB').save("/tmp/icons_preview.png")
print("preview saved /tmp/icons_preview.png")

# ---- emit header ----
hdr="// Auto-generated colored RGB565A8 icons for the status page (v2.1).\n"
hdr+="// Generated by tools/gen_status_icons.py (PIL supersample -> RGB565A8).\n"
hdr+="// Union Jack + 7 weather-condition icons + 5 agent icons. Color preserved.\n#pragma once\n#include <stdint.h>\n\n"
for base,sym,im in allicons:
    w,h=im.size
    hdr+=c_array(sym,base+"_W",base+"_H",w,h,to_rgb565a8(im))+"\n"
# Write into the repo's firmware/src/, resolved relative to this script (tools/).
_repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_out_path = os.path.join(_repo_root, "firmware", "src", "icons_status.h")
with open(_out_path, "w") as f:
    f.write(hdr)
print("header written firmware/src/icons_status.h")
print("icons:",[b for b,_,_ in allicons])
