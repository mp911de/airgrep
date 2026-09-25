#!/usr/bin/env python3
"""Placement B drawing. Front view, origin upper-left, +Y down, mm. Run: python3 mechanical/draw_placement_b.py"""
import json, math
W, H = 105.0, 95.0
S = 10  # px per mm
PAD = 20
out = {"revision": "B", "units": "mm", "origin": "upper-left, front view, +Y down", "board": {"width": W, "height": H}}

# ---- modules (envelope x,y,w,h) ----
gnss = dict(ref="GNSS", x=4.0, y=15.0, w=30.48, h=38.10, color="#c7e1f6", note="GPS-18037, SMA up, manufacturer geometry")
disp = dict(ref="DISPLAY", x=39.3, y=16.0, w=26.4, h=51.4, color="#bce8d7", note="100069905 assembly envelope, display-facing mirror")
ads  = dict(ref="ADSBEE", x=72.0, y=15.0, w=30.0, h=48.0, color="#f4d4ce", note="48x30 image estimate, portrait, USB faces down; unplug for USB service (OTA available)")
zones = [gnss, disp, ads,
  dict(ref="POWER", x=66.0, y=68.0, w=36.0, h=19.0, color="#ffddaa", note="BQ25606, TPS61088, TPS22975, fuse, supervisor; 5 mm below ADSBee, 40 mm from GNSS"),
  dict(ref="USB_C", x=86.0, y=88.0, w=10.0, h=7.0, color="#cbd5e1", note="charging receptacle, bottom edge"),
  dict(ref="LED", x=81.0, y=90.0, w=3.0, h=3.0, color="#a3e635", note="bi-colour: blue charging, green running"),
  dict(ref="POWER_SW", x=69.0, y=91.5, w=9.0, h=3.5, color="#b7c6d6", note="JS102011SAQN, actuator down"),
  dict(ref="GAUGE_LOGIC", x=7.0, y=56.0, w=22.0, h=15.0, color="#dacbea", note="MAX17048, TUSB320 ILIM helper, NPN, supervisor"),
  dict(ref="BUZZER", x=7.0, y=73.0, w=14.0, h=14.0, color="#c9d4f1", note="CPT-9019S + driver"),
  dict(ref="MUTE_SW", x=0.0, y=62.0, w=3.5, h=9.0, color="#b7c6d6", note="JS102011SAQN, actuator left"),
  dict(ref="TEMP_HDR", x=23.0, y=73.0, w=14.0, h=7.0, color="#dacbea", note="J_NTC, J_THERM"),
  dict(ref="BAT_CONN", x=23.0, y=82.0, w=14.0, h=8.0, color="#dacbea", note="J_BAT + fuse, rear-cover access"),
  dict(ref="SHUNTS_TP", x=39.3, y=70.0, w=24.0, h=14.0, color="#e2e8f0", note="JP_PWR_ESP, JP_PWR_ADS, test pads"),
]
holes = [dict(ref=f"H{i+1}", x=x, y=y, drill=2.8, keepout=5.0) for i,(x,y) in enumerate([(3.5,3.5),(101.5,3.5),(3.5,91.5),(101.5,91.5),(52.5,91.5)])]
ant = [dict(ref="GNSS_RF", x=40.0, y=7.0), dict(ref="868_RF", x=62.0, y=7.0), dict(ref="1090_RF", x=92.0, y=7.0)]
# GNSS SMA axis: native y=15.24 -> x = 34.48-15.24; projects ~6.1 mm above module top edge
sma = dict(x=gnss["x"]+30.48-15.24, y_tip=gnss["y"]-6.1, y_base=gnss["y"])
ufl = dict(x=ads["x"]+4.0, y=ads["y"]+2.0)  # ADSBee 1090/SUBG U.FL, image estimate
for a in ant: a.update(drill=6.5, keepout=11.0, provisional=True)
battery = dict(x=(W-100)/2, y=(H-60)/2, w=100, h=60, thickness=11, side="rear")

# ---- header pads (derived from manufacturer coordinates, see placement-C.json / findings) ----
pads = []
def row(ref, x0, y0, n, dx, dy, src):
    for i in range(n): pads.append(dict(ref=ref, number=str(i+1), x=round(x0+i*dx,3), y=round(y0+i*dy,3), source=src))
row("J_GNSS", gnss["x"]+6.35, gnss["y"]+36.83, 8, 2.54, 0, "SparkFun native board")
dx, dy = disp["x"]-42.0, disp["y"]-18.0  # offsets from placement C reference
row("J_ESP_A", 66.2+dx, 21.88+dy, 8, 0, 2.54, "Seeed native, mirrored")
row("J_ESP_B", 44.2+dx, 21.88+dy, 7, 0, 2.54, "Seeed native, mirrored")
row("J_ESP_C", 66.2+dx, 55.746+dy, 4, 0, 2.54, "Seeed native, mirrored")
row("J_ESP_D", 44.2+dx, 58.286+dy, 2, 0, 2.54, "Seeed native, mirrored")
row("J_ADS_MAIN", ads["x"]+27.8, ads["y"]+38.6, 13, 0, -2.54, "image estimate")
row("J_ADS_GNSS", ads["x"]+9.9, ads["y"]+1.2, 5, 2.54, 0, "image estimate")
mholes = []
for (x,y) in [(2.54,2.54),(27.94,2.54),(2.54,35.56),(27.94,35.56)]: mholes.append(("GNSS", gnss["x"]+x, gnss["y"]+y, 3.3))
for (x,y) in [(3.2,25.7),(23.2,25.7),(3.2,48.2),(23.2,48.2)]: mholes.append(("DISPLAY", disp["x"]+x, disp["y"]+y, 2.5))
for (x,y) in [(3.9,3.1),(3.9,43.5),(26.5,3.1),(26.5,43.5)]: mholes.append(("ADSBEE", ads["x"]+x, ads["y"]+y, 2.5))

# ---- checks ----
def ov(a,b): return not (a["x"]+a["w"]<=b["x"] or b["x"]+b["w"]<=a["x"] or a["y"]+a["h"]<=b["y"] or b["y"]+b["h"]<=a["y"])
issues=[]
for i,a in enumerate(zones):
    for b in zones[i+1:]:
        if ov(a,b): issues.append(f"overlap {a['ref']} / {b['ref']}")
    if a["x"]<0 or a["y"]<0 or a["x"]+a["w"]>W or a["y"]+a["h"]>H: issues.append(f"{a['ref']} outside board")
for h in holes+ant:
    for z in zones:
        if z["ref"] in ("MUTE_SW",): continue
        r=h["keepout"]/2
        if ov(dict(x=h["x"]-r,y=h["y"]-r,w=2*r,h=2*r), z): issues.append(f"{h['ref']} keepout hits {z['ref']}")
def dist(a,b):
    ddx=max(b["x"]-(a["x"]+a["w"]), a["x"]-(b["x"]+b["w"]), 0); ddy=max(b["y"]-(a["y"]+a["h"]), a["y"]-(b["y"]+b["h"]), 0); return math.hypot(ddx,ddy)
pw=zones[3]
out.update(gnss_sma=sma, adsbee_ufl=ufl, zones=zones, holes=holes, antenna_mounts=ant, battery=battery, header_pads=pads,
  module_holes=[dict(module=m,x=round(x,3),y=round(y,3),drill=d) for m,x,y,d in mholes],
  distances={"POWER_to_GNSS_mm": round(dist(pw,gnss),1), "POWER_to_ADSBEE_mm": round(dist(pw,ads),1)},
  checks=["zones inside board","no zone overlap","hole keepouts clear"], issues=issues,
  not_checked=["package courtyards","ADSBee real geometry","coax bends","socket heights","routing"])
json.dump(out, open("mechanical/placement-B.json","w"), indent=1)

# ---- svg ----
def X(v): return PAD+v*S
def Y(v): return PAD+v*S
o=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W*S+2*PAD+330}" height="{W*S+2*PAD+330}" viewBox="0 0 {W*S+2*PAD+330} {W*S+2*PAD+330}" font-family="system-ui,sans-serif" font-size="11">',
   f'<rect width="100%" height="100%" fill="#f6f8fb"/>',
   f'<rect x="{X(battery["x"])}" y="{Y(battery["y"])}" width="{battery["w"]*S}" height="{battery["h"]*S}" fill="none" stroke="#b45309" stroke-dasharray="6 4"/>',
   f'<text x="{X(battery["x"])+4}" y="{Y(battery["y"])-4}" fill="#b45309">battery 100 x 60 x 11 behind PCB</text>',
   f'<rect x="{X(0)}" y="{Y(0)}" width="{W*S}" height="{H*S}" rx="20" fill="#eaf3ea" stroke="#1f4d3a" stroke-width="2"/>']
for z in zones:
    o.append(f'<rect x="{X(z["x"])}" y="{Y(z["y"])}" width="{z["w"]*S}" height="{z["h"]*S}" rx="6" fill="{z["color"]}" stroke="#334155" stroke-dasharray="{"4 3" if z["ref"]=="ADSBEE" else "none"}"/>')
    cx=X(z["x"]+z["w"]/2); cy=Y(z["y"]+z["h"]/2)
    o.append(f'<text x="{cx}" y="{cy-2}" text-anchor="middle" fill="#1e293b" font-weight="600">{z["ref"]}</text>')
    o.append(f'<text x="{cx}" y="{cy+10}" text-anchor="middle" fill="#475569" font-size="9">{z["w"]:g} x {z["h"]:g}</text>')
for h in holes:
    o.append(f'<circle cx="{X(h["x"])}" cy="{Y(h["y"])}" r="{h["keepout"]/2*S}" fill="none" stroke="#94a3b8"/><circle cx="{X(h["x"])}" cy="{Y(h["y"])}" r="{h["drill"]/2*S}" fill="#fff" stroke="#334155"/>')
for a in ant:
    o.append(f'<circle cx="{X(a["x"])}" cy="{Y(a["y"])}" r="{a["keepout"]/2*S}" fill="none" stroke="#b45309"/><circle cx="{X(a["x"])}" cy="{Y(a["y"])}" r="{a["drill"]/2*S}" fill="#fff" stroke="#334155"/><text x="{X(a["x"])-14}" y="{Y(a["y"])+3}" font-size="8">{a["ref"].split("_")[0]}</text>')
o.append(f'<rect x="{X(sma["x"]-3.2)}" y="{Y(sma["y_tip"])}" width="{6.4*S}" height="{(sma["y_base"]-sma["y_tip"])*S}" fill="#c9a227" stroke="#7c5a00"/><text x="{X(sma["x"])}" y="{Y(sma["y_tip"])-4}" text-anchor="middle" font-size="9">GNSS SMA</text>')
o.append(f'<circle cx="{X(ufl["x"])}" cy="{Y(ufl["y"])}" r="{1.5*S}" fill="#c9a227" stroke="#7c5a00"/><text x="{X(ufl["x"])+18}" y="{Y(ufl["y"])+4}" font-size="9">U.FL x2</text>')
for m,x,y,d in mholes: o.append(f'<circle cx="{X(x)}" cy="{Y(y)}" r="{d/2*S}" fill="#fff" stroke="#334155"/>')
for p in pads: o.append(f'<circle cx="{X(p["x"])}" cy="{Y(p["y"])}" r="{0.75*S}" fill="#d4a017" stroke="#7c5a00"/>')
# side text
tx=W*S+2*PAD+10; ty=Y(6)
lines=[("Placement B | 105 x 95 mm | front view","600"),("",""),("RF at top, converters bottom right",""),
 (f"POWER zone to GNSS: {out['distances']['POWER_to_GNSS_mm']} mm",""),(f"POWER zone to ADSBee: {out['distances']['POWER_to_ADSBEE_mm']} mm",""),
 ("POWER slider, USB-C, LED: bottom right, facing down",""),("MUTE slider: left side",""),("Display centred, glass front",""),
 ("ADSBee portrait, USB down: unplug for USB service",""),("Shunts + test pads below display",""),("",""),
 ("Dashed = image-estimated geometry",""),("Circles = holes; brown = antenna NPTH, provisional",""),("Antenna mounts offset >= 20 mm from GNSS SMA axis",""),("",""),
 ("Issues: "+("; ".join(issues) if issues else "none"),"600")]
for i,(t,w) in enumerate(lines): o.append(f'<text x="{tx}" y="{ty+i*16}" font-weight="{w or 400}">{t}</text>')
o.append(f'<text x="{X(0)}" y="{Y(H)+22}" fill="#475569">Not routed, not for manufacture. Coordinates in placement-B.json.</text>')
o.append('</svg>')
open("mechanical/placement-B.svg","w").write("\n".join(o))
print("distances", out["distances"], "issues", issues)
