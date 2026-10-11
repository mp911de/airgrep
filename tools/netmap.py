"""Net-coloured copper maps: one SVG per copper layer, the whole board.

    tools/kpy tools/netmap.py [--project NAME] [--out DIR] [--layers F.Cu,In2.Cu]

Tracks, vias, pads and zone fills in one colour per net (GND grey), rule areas dotted red,
references as text, a legend of the nets on the layer. Output DIR/netmap_<layer>.svg,
default hardware/NAME/netmap/. Units in the SVG are mil; the viewBox is the board outline.
"""
import argparse
import colorsys
import os
import sys

import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import project  # noqa: E402

MIL = 25400.0


def m(v):
    return v / MIL


def colour(net, table):
    if net == "GND":
        return "#9a9a9a"
    if not net:
        return "#dddddd"
    if net not in table:
        h = (len(table) * 0.618034) % 1.0
        r, g, b = colorsys.hls_to_rgb(h, 0.45, 0.85)
        table[net] = "#%02x%02x%02x" % (int(r * 255), int(g * 255), int(b * 255))
    return table[net]


def poly_points(poly):
    return " ".join("%.1f,%.1f" % (m(poly.CVertex(i).x), m(poly.CVertex(i).y)) for i in range(poly.FullPointCount()))


def chain_points(chain):
    return " ".join("%.1f,%.1f" % (m(chain.CPoint(k).x), m(chain.CPoint(k).y)) for k in range(chain.PointCount()))


def layer_svg(board, lid, name):
    nets = {}
    out = []
    bb = board.GetBoardEdgesBoundingBox()
    x0, y0, w, h = m(bb.GetLeft()), m(bb.GetTop()), m(bb.GetWidth()), m(bb.GetHeight())
    # zones: filled copper if present, else the outline; rule areas dotted
    for z in board.Zones():
        if not z.IsOnLayer(lid):
            continue
        if z.GetIsRuleArea():
            o = z.Outline()
            for i in range(o.OutlineCount()):
                out.append('<polygon points="%s" fill="none" stroke="#d00" stroke-width="2" stroke-dasharray="8,6"/>'
                           % chain_points(o.Outline(i)))
            continue
        c = colour(z.GetNetname(), nets)
        filled = z.GetFilledPolysList(lid)
        if filled.OutlineCount():
            for i in range(filled.OutlineCount()):
                pts = chain_points(filled.Outline(i))
                holes = "".join(" M " + chain_points(filled.Hole(i, k)).replace(" ", " L ").replace(",", " ") + " Z"
                                for k in range(filled.HoleCount(i)))
                out.append('<path d="M %s Z%s" fill="%s" fill-opacity="0.35" fill-rule="evenodd" stroke="%s" stroke-width="1"/>'
                           % (pts.replace(" ", " L ").replace(",", " "), holes, c, c))
        else:
            o = z.Outline()
            for i in range(o.OutlineCount()):
                out.append('<polygon points="%s" fill="%s" fill-opacity="0.15" stroke="%s" stroke-width="1.5"/>'
                           % (chain_points(o.Outline(i)), c, c))
    for t in board.GetTracks():
        if t.GetClass() == "PCB_VIA":
            continue
        if t.GetLayer() != lid:
            continue
        s, e = t.GetStart(), t.GetEnd()
        out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="%.1f" stroke-linecap="round"/>'
                   % (m(s.x), m(s.y), m(e.x), m(e.y), colour(t.GetNetname(), nets), m(t.GetWidth())))
    for f in board.GetFootprints():
        for p in f.Pads():
            if not p.IsOnLayer(lid):
                continue
            out.append('<polygon points="%s" fill="%s" fill-opacity="0.9"/>'
                       % (poly_points(p.GetEffectivePolygon(lid)), colour(p.GetNetname(), nets)))
    for t in board.GetTracks():
        if t.GetClass() != "PCB_VIA" or not t.IsOnLayer(lid):
            continue
        s = t.GetStart()
        out.append('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s"/><circle cx="%.1f" cy="%.1f" r="%.1f" fill="#fff"/>'
                   % (m(s.x), m(s.y), m(t.GetWidth(lid)) / 2, colour(t.GetNetname(), nets), m(s.x), m(s.y), m(t.GetDrill()) / 2))
    for f in board.GetFootprints():
        if f.GetLayer() != (pcbnew.F_Cu if lid != pcbnew.B_Cu else pcbnew.B_Cu) and lid in (pcbnew.F_Cu, pcbnew.B_Cu):
            continue
        c = f.GetPosition()
        out.append('<text x="%.1f" y="%.1f" font-size="22" text-anchor="middle" fill="#000">%s</text>'
                   % (m(c.x), m(c.y), f.GetReference()))
    legend = ['<g transform="translate(%.1f,%.1f)">' % (x0 + w + 20, y0)]
    for k, (net, c) in enumerate(sorted(nets.items())):
        legend.append('<rect x="0" y="%d" width="30" height="18" fill="%s"/><text x="38" y="%d" font-size="18">%s</text>'
                      % (k * 24, c, k * 24 + 15, net))
    legend.append("</g>")
    lw = 40 + 8 * max([len(n) for n in nets] + [10])
    head = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="%.1f %.1f %.1f %.1f" width="%.0f" height="%.0f" font-family="sans-serif">'
            '<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#fff"/>'
            '<text x="%.1f" y="%.1f" font-size="36">%s</text>'
            % (x0 - 20, y0 - 60, w + lw + 60, h + 80, (w + lw + 60) / 4, (h + 80) / 4,
               x0 - 20, y0 - 60, w + lw + 60, h + 80, x0, y0 - 20, name))
    return head + "".join(out) + "".join(legend) + "</svg>"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    project.add_argument(ap)
    ap.add_argument("--out", metavar="DIR", help="output folder (default <project>/netmap)")
    ap.add_argument("--layers", metavar="LIST", help="comma list of copper layers (default all)")
    a = ap.parse_args()
    path = project.resolve(a.project)
    board = pcbnew.LoadBoard(project.pcb(path))
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    out = a.out or os.path.join(path, "netmap")
    os.makedirs(out, exist_ok=True)
    layers = a.layers.split(",") if a.layers else [board.GetLayerName(l) for l in board.GetEnabledLayers().CuStack()]
    for name in layers:
        lid = board.GetLayerID(name)
        if lid < 0:
            raise SystemExit("no layer " + name)
        target = os.path.join(out, "netmap_%s.svg" % name.replace(".", "_"))
        with open(target, "w", encoding="utf-8") as fh:
            fh.write(layer_svg(board, lid, name))
        print(target)


if __name__ == "__main__":
    main()
