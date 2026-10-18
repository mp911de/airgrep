#!/usr/bin/env python3
"""Report straight and arc copper tracks by width, layer and net."""
from collections import defaultdict

from pcb_report import labels, load_board, mil, pcbnew, table


def main():
    board = load_board(__doc__)
    groups = defaultdict(list)
    for track in board.GetTracks():
        if track.GetClass() != "PCB_VIA":
            groups[(track.GetWidth(), board.GetLayerName(track.GetLayer()))].append(
                track.GetNetname() or "(no net)")
    table("Tracks, including arcs", ["Width", "Layer", "Segments", "Nets"],
          [(mil(width), layer, len(nets), labels(nets))
           for (width, layer), nets in sorted(groups.items())])


if __name__ == "__main__":
    main()
