#!/usr/bin/env python3
"""Make tabular Inter files from the variable font.

The optical size equals the pixel size it is used at
The font converter ignores OpenType features, so the "tnum" substitution is applied to the character map here. Digits then share one width and a
changing number does not move. Needs fontTools and the variable Inter of Google Fonts.
"""

import argparse
from pathlib import Path

from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

# Optical size and weight. One entry per <bin> font in globals.xml.
INSTANCES = [(16, 600), (20, 500), (20, 600), (24, 600), (30, 500), (30, 600), (120, 600)]
# Where the optical size axis of Inter ends. A larger font takes this one.
OPSZ_MAX = 32


def tabular(font):
    gsub = font["GSUB"].table
    lookups = {index for feature in gsub.FeatureList.FeatureRecord if
               feature.FeatureTag == "tnum"
               for index in feature.Feature.LookupListIndex}
    mapping = {}
    for index in lookups:
        for subtable in gsub.LookupList.Lookup[index].SubTable:
            mapping.update(getattr(subtable, "mapping", {}))
    for table in font["cmap"].tables:
        for code, glyph in table.cmap.items():
            table.cmap[code] = mapping.get(glyph, glyph)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path,
                        default=Path("/Library/Fonts/Inter-VariableFont_opsz,wght.ttf"))
    args = parser.parse_args()
    for size, weight in INSTANCES:
        font = instancer.instantiateVariableFont(TTFont(args.source),
                                                 {"opsz": min(size, OPSZ_MAX),
                                                  "wght": weight})
        tabular(font)
        target = Path(__file__).parent / f"InterTabular-{size}-{weight}.ttf"
        font.save(target)
        print(target.name)


if __name__ == "__main__":
    main()
