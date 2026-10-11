#!/usr/bin/env python3
"""Read-only queries on the carrier board through pcbnew. Loads the board and never saves it.

  tools/board.py nets [FILTER]          net names with pad count
  tools/board.py part U4                position, rotation, layer, value, footprint, pads
  tools/board.py net VSYS               pads, tracks, vias and zones on one net
  tools/board.py window X0 Y0 X1 Y1     pads, tracks, vias and zones inside a rectangle
  tools/board.py widths                 histogram of track widths and via sizes
  tools/board.py render X0 Y0 X1 Y1 --layer F.Cu --out FILE.png    cropped plot of a rectangle

Output is one line per item. Units are mil, rounded to integers. Widths and pad sizes that are
not a whole mil keep 2 decimals. --mm switches to mm with 3 decimals, and the window arguments
follow the same unit. --project NAME reads hardware/NAME (default $CARRIER_PROJECT or
carrier-ti-charger), --pcb FILE reads another board file.
render plots a temporary copy of the board with kicad-cli and crops it with pdftoppm.
Runs from any directory. Without pcbnew it restarts itself through tools/kpy.
"""
import argparse, collections, os, shutil, signal, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
try:
    import pcbnew
except ImportError:
    if os.environ.get("KPY"):
        sys.exit("board.py: KiCad's Python has no pcbnew")
    kpy = os.path.join(HERE, "kpy")
    os.execv(kpy, [kpy, os.path.abspath(__file__)] + sys.argv[1:])

sys.path.insert(0, HERE)
import project  # noqa: E402

MIL = 25400          # nm per mil
MM = 1000000         # nm per mm
unit = MIL


def u(nm):
    """Length in the output unit."""
    return f"{nm / MM:.3f}" if unit == MM else str(round(nm / MIL))


def sz(nm):
    """Width, diameter or pad size. Mil output keeps 2 decimals when the value is not a whole mil."""
    if unit == MM or abs(nm / MIL - round(nm / MIL)) < 0.005:
        return u(nm)
    return f"{nm / MIL:.2f}"


def pt(p):
    return f"{u(p.x)},{u(p.y)}"


def is_via(t):
    return t.GetClass() == "PCB_VIA"


def via_size(v):
    # pcbnew 10 wants a layer for the via diameter. GetWidth() without one is deprecated.
    return f"{sz(v.GetWidth(pcbnew.F_Cu))}/{sz(v.GetDrillValue())}"


def cu_layers(b, item):
    """Copper layer names of a zone or pad, in stack order."""
    return [b.GetLayerName(l) for l in item.GetLayerSet().CuStack()]


def pad_layer(b, pad):
    names = cu_layers(b, pad)
    return "*.Cu" if len(names) > 1 else (names[0] if names else "-")


def pad_line(b, ref, pad):
    s = pad.GetSize()
    # The size is in the pad's own axes. The rotation is printed when it is not 0.
    rot = pad.GetOrientationDegrees()
    return (f"pad {ref}.{pad.GetNumber()} {pad.GetNetname() or '-'} {pad_layer(b, pad)} "
            f"{pt(pad.GetPosition())} {sz(s.x)}x{sz(s.y)}" + (f" rot {rot:g}" if rot else ""))


def track_line(b, t):
    if is_via(t):
        return (f"via {t.GetNetname() or '-'} {pt(t.GetPosition())} {via_size(t)} "
                f"{b.GetLayerName(t.TopLayer())}-{b.GetLayerName(t.BottomLayer())}")
    kind = "arc" if t.GetClass() == "PCB_ARC" else "track"
    return (f"{kind} {t.GetNetname() or '-'} {b.GetLayerName(t.GetLayer())} "
            f"{pt(t.GetStart())} > {pt(t.GetEnd())} w {sz(t.GetWidth())} len {u(t.GetLength())}")


def zone_name(z):
    return f' "{z.GetZoneName()}"' if z.GetZoneName() else ""


def cmd_nets(b, a):
    pads = collections.Counter(p.GetNetname() for f in b.GetFootprints() for p in f.Pads())
    flt = (a.filter or "").lower()
    for name in sorted(str(n) for n in b.GetNetsByName().keys()):
        if name and flt in name.lower():
            print(name, pads[name])


def cmd_part(b, a):
    f = b.FindFootprintByReference(a.ref)
    if not f:
        sys.exit(f"no footprint {a.ref}")
    print(f"{a.ref} {f.GetValue()} {f.GetFPIDAsString()} at {pt(f.GetPosition())} "
          f"rot {f.GetOrientationDegrees():g} {b.GetLayerName(f.GetLayer())}")
    for p in f.Pads():
        print(pad_line(b, a.ref, p))


def cmd_net(b, a):
    if not b.FindNet(a.name):
        near = [str(n) for n in b.GetNetsByName().keys() if a.name.lower() in str(n).lower()]
        sys.exit(f"no net {a.name}. Similar: {' '.join(sorted(near)) or 'none'}")
    for f in b.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() == a.name:
                print(pad_line(b, f.GetReference(), p))
    length = collections.Counter()
    for t in b.GetTracks():
        if t.GetNetname() == a.name:
            print(track_line(b, t))
            if not is_via(t):
                length[b.GetLayerName(t.GetLayer())] += t.GetLength()
    for z in b.Zones():
        if z.GetNetname() == a.name:
            print(f"zone {a.name} {','.join(cu_layers(b, z))}{zone_name(z)}")
    for layer, nm in sorted(length.items()):
        print(f"total {layer} len {u(nm)}")


def rect_poly(x0, y0, x1, y1):
    r = pcbnew.SHAPE_POLY_SET()
    r.NewOutline()
    for x, y in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        r.Append(int(x), int(y))
    return r


def cmd_window(b, a):
    x0, x1 = sorted((a.x0 * unit, a.x1 * unit))
    y0, y1 = sorted((a.y0 * unit, a.y1 * unit))
    box = pcbnew.BOX2I(pcbnew.VECTOR2I(int(x0), int(y0)), pcbnew.VECTOR2I(int(x1 - x0), int(y1 - y0)))
    for f in b.GetFootprints():
        for p in f.Pads():
            if p.GetBoundingBox().Intersects(box):
                print(pad_line(b, f.GetReference(), p))
    for t in b.GetTracks():
        if t.HitTest(box, False):
            print(track_line(b, t))
    # Zones report the filled copper inside the window as a share of the window area.
    # A zone outline alone says nothing here, a plane can enclose the window without touching it.
    area = (x1 - x0) * (y1 - y0)
    for z in b.Zones():
        if z.GetIsRuleArea():
            if z.GetBoundingBox().Intersects(box):
                print(f"zone keepout {','.join(cu_layers(b, z))}{zone_name(z)}")
            continue
        for l in z.GetLayerSet().CuStack():
            fill = pcbnew.SHAPE_POLY_SET(z.GetFilledPolysList(l))
            fill.BooleanIntersection(rect_poly(x0, y0, x1, y1))
            if fill.Area() > 0:
                print(f"zone {z.GetNetname() or '-'} {b.GetLayerName(l)} "
                      f"fill {100 * fill.Area() / area:.0f}%{zone_name(z)}")


def cmd_widths(b, a):
    tracks, length, vias = collections.Counter(), collections.Counter(), collections.Counter()
    for t in b.GetTracks():
        if is_via(t):
            vias[(t.GetWidth(pcbnew.F_Cu), t.GetDrillValue())] += 1
        else:
            tracks[t.GetWidth()] += 1
            length[t.GetWidth()] += t.GetLength()
    for w in sorted(tracks):
        print(f"track w {sz(w)} count {tracks[w]} len {u(length[w])}")
    for (d, drill) in sorted(vias):
        print(f"via {sz(d)}/{sz(drill)} count {vias[(d, drill)]}")


def cmd_render(pcb, a):
    """Plot the layers to PDF at scale 1 and cut the rectangle out as PNG."""
    cli = shutil.which("kicad-cli") or "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
    ppm = shutil.which("pdftoppm")
    if not os.path.exists(cli) or not ppm:
        sys.exit("render needs kicad-cli and pdftoppm (brew install poppler)")
    x0, x1 = sorted((a.x0 * unit / MM / 25.4, a.x1 * unit / MM / 25.4))    # inch
    y0, y1 = sorted((a.y0 * unit / MM / 25.4, a.y1 * unit / MM / 25.4))
    dpi = max(300, min(9600, round(1600 / max(x1 - x0, y1 - y0))))         # long side near 1600 px
    out = a.out[:-4] if a.out.lower().endswith(".png") else a.out
    # kicad-cli runs on a copy. Nothing is written next to the real board.
    with tempfile.TemporaryDirectory() as tmp:
        copy, pdf = os.path.join(tmp, "board.kicad_pcb"), os.path.join(tmp, "board.pdf")
        shutil.copy(pcb, copy)
        r = subprocess.run([cli, "pcb", "export", "pdf", "--mode-single", "-l", a.layer, "-o", pdf, copy],
                           capture_output=True, text=True)
        if r.returncode or not os.path.exists(pdf):
            sys.exit(f"kicad-cli failed: {(r.stderr or r.stdout).strip()}")
        subprocess.run([ppm, "-png", "-singlefile", "-r", str(dpi), "-x", str(round(x0 * dpi)),
                        "-y", str(round(y0 * dpi)), "-W", str(round((x1 - x0) * dpi)),
                        "-H", str(round((y1 - y0) * dpi)), pdf, out], check=True)
    print(f"{out}.png {a.layer} {dpi} dpi")


def main():
    global unit
    # The shared options are accepted before and after the subcommand.
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--pcb", default=argparse.SUPPRESS, metavar="FILE",
                        help="board file (default the project's carrier.kicad_pcb)")
    common.add_argument("--project", default=argparse.SUPPRESS, metavar="NAME",
                        help="project under hardware/ (default $CARRIER_PROJECT or %s)" % project.DEFAULT)
    common.add_argument("--mm", action="store_true", default=argparse.SUPPRESS,
                        help="mm with 3 decimals instead of integer mil")
    ap = argparse.ArgumentParser(description="Read-only queries on a KiCad board.", parents=[common])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("nets", parents=[common], help="net names with pad count")
    p.add_argument("filter", nargs="?", help="substring, case-insensitive")
    p = sub.add_parser("part", parents=[common], help="one footprint and its pads")
    p.add_argument("ref")
    p = sub.add_parser("net", parents=[common], help="pads, tracks, vias and zones on one net")
    p.add_argument("name")
    p = sub.add_parser("window", parents=[common], help="items inside a rectangle")
    for n in ("x0", "y0", "x1", "y1"):
        p.add_argument(n, type=float)
    sub.add_parser("widths", parents=[common], help="histogram of track widths and via sizes")
    p = sub.add_parser("render", parents=[common], help="cropped PNG plot of a rectangle")
    for n in ("x0", "y0", "x1", "y1"):
        p.add_argument(n, type=float)
    p.add_argument("--layer", required=True, help="layer name, or a comma list such as F.Cu,Edge.Cuts")
    p.add_argument("--out", required=True, metavar="FILE", help="PNG file to write")
    a = ap.parse_args()

    signal.signal(signal.SIGPIPE, signal.SIG_DFL)   # quiet exit under `| head`
    unit = MM if getattr(a, "mm", False) else MIL
    pcb = getattr(a, "pcb", None) or project.pcb(project.resolve(getattr(a, "project", None)))
    if not os.path.isfile(pcb):
        sys.exit(f"no board file {pcb}")
    if a.cmd == "render":
        return cmd_render(pcb, a)
    b = pcbnew.LoadBoard(pcb)
    print(f"# {os.path.basename(pcb)}, unit {'mm' if unit == MM else 'mil'}")
    globals()["cmd_" + a.cmd](b, a)


if __name__ == "__main__":
    main()
