#!/usr/bin/env python3
"""Report vias and footprint drills by size, with net and pad labels."""
from collections import defaultdict

from pcb_report import labels, load_board, mil, pcbnew, table


def main():
    board = load_board(__doc__)
    vias = defaultdict(list)
    holes = defaultdict(list)
    for via in board.GetTracks():
        if via.GetClass() != "PCB_VIA":
            continue
        layers = list(via.GetLayerSet().CuStack())
        diameters = tuple(via.GetWidth(layer) for layer in layers)
        span = f"{board.GetLayerName(via.TopLayer())}-{board.GetLayerName(via.BottomLayer())}"
        vias[(via.GetDrillValue(), diameters, span)].append(via.GetNetname() or "(no net)")
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            drill = pad.GetDrillSize()
            if not drill.x and not drill.y:
                continue
            kind = "NPTH" if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH else "PTH"
            shape = "slot" if pad.GetDrillShape() == pcbnew.PAD_DRILL_SHAPE_OBLONG else "round"
            holes[(kind, drill.x, drill.y, shape)].append(
                (f"{footprint.GetReference()}.{pad.GetNumber() or '(unnumbered)'}",
                 pad.GetNetname() or "(no net)"))
    for span in sorted({key[2] for key in vias}):
        rows = []
        for (drill, diameters, layer_span), nets in sorted(vias.items()):
            if layer_span != span:
                continue
            low, high = min(diameters), max(diameters)
            diameter = mil(low) if low == high else f"{mil(low)}..{mil(high)}"
            rows.append((f"{diameter} / {mil(drill)}", mil((low - drill) / 2),
                         len(nets), labels(nets)))
        table(f"Vias {span}, smallest drill first",
              ["Copper / drill", "Min ring", "Count", "Nets"], rows)
    if not vias:
        table("Vias", [], [])
    for kind in ("PTH", "NPTH"):
        rows = []
        for (plating, x, y, shape), items in sorted(holes.items(), key=lambda item: (item[0][1:])):
            if plating == kind:
                size = mil(x) if shape == "round" else f"{mil(x)} x {mil(y)} slot"
                rows.append((size, len(items), labels(pad for pad, net in items),
                             labels(net for pad, net in items)))
        table(f"{kind} footprint holes, smallest X drill first",
              ["Drill", "Count", "Pads", "Nets"], rows)
    print("\nVia ring is nominal (copper diameter - drill) / 2, minimum across layers.")
    print("Footprint holes are drills only; Edge.Cuts cutouts are not included.")


if __name__ == "__main__":
    main()
