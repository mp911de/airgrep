"""Shared read-only board loading and ASCII tables for fabrication reports."""
import argparse
import os
from pathlib import Path
import sys
import textwrap

try:
    import pcbnew
except ImportError:
    runner = Path(__file__).with_name("kpy")
    if os.environ.get("KPY") or not runner.exists():
        sys.exit("pcbnew unavailable; run with KiCad's Python")
    os.execv(str(runner), [str(runner), *sys.argv])


def load_board(description):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("pcb", type=Path, help="path to a .kicad_pcb file")
    path = parser.parse_args().pcb
    if not path.is_file():
        parser.error(f"no board file: {path}")
    board = pcbnew.LoadBoard(str(path.resolve()))
    print(f"Board: {path.resolve()}\nUnits: mil. Grouping uses exact stored dimensions.")
    return board


def mil(value):
    return f"{value / 25400:.4f}".rstrip("0").rstrip(".")


def labels(values):
    return ", ".join(sorted(set(values)))


def table(title, headers, rows):
    print(f"\n{title}")
    if not rows:
        print("(none)")
        return
    wrapped = [[textwrap.wrap(str(cell), width=64, break_long_words=False,
                              break_on_hyphens=False) or [""] for cell in row]
               for row in [headers, *rows]]
    widths = [max(len(line) for row in wrapped for line in row[i])
              for i in range(len(headers))]
    border = "+" + "+".join("-" * (w + 2) for w in widths) + "+"
    print(border)
    for index, row in enumerate(wrapped):
        for line in range(max(map(len, row))):
            print("| " + " | ".join((cell[line] if line < len(cell) else "").ljust(w)
                                     for cell, w in zip(row, widths)) + " |")
        if index == 0:
            print(border)
    print(border)
