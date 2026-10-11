"""Text-level editing of KiCad 10 schematic files (.kicad_sch).

A schematic is a flat list of top-level s-expression items (version, generator, uuid, paper,
title_block, lib_symbols, symbol instances, wires, labels, junctions, sheet_instances; keep the
first five first). This module splits a file into those items as text, so edits
keep KiCad's own formatting for everything that is not touched, and builds new items from
templates. KiCad reformats on save, so byte-identical output is not a goal, a small diff is.

    sch = Schematic.load(path)
    sch.remove(lambda item: item.ref() == "R6")
    sch.add(symbol_instance(...)); sch.add(global_label("CE", 50, 60, 0))
    sch.save(path)

Coordinates are mm, as in the file. Grid is 1.27 mm; pin ends must land on it.
"""
import os
import re
import uuid


def new_uuid():
    return str(uuid.uuid4())


def split_items(text):
    """Top-level items of the file body as text, plus the header and trailer around them."""
    depth, start, items = 0, None, []
    in_str = False
    i = 0
    n = len(text)
    first = text.index("\n\t(") + 1  # after "(kicad_sch"
    head = text[:first]
    i = first
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            if depth == 0:
                start = i
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                items.append(text[start:i + 1])
                start = None
            elif depth < 0:
                return head, items, text[i:]
        i += 1
    raise ValueError("unbalanced schematic file")


class Item(str):
    """One top-level block as text with a few accessors."""

    def kind(self):
        return re.match(r"\(\s*([\w]+)", self).group(1)

    def prop(self, name):
        m = re.search(r'\(property "%s" "([^"]*)"' % re.escape(name), self)
        return m.group(1) if m else None

    def ref(self):
        return self.prop("Reference") if self.kind() == "symbol" else None

    def lib_id(self):
        m = re.search(r'\(lib_id "([^"]+)"', self)
        return m.group(1) if m else None

    def at(self):
        m = re.search(r"\n\t\t\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", self)
        return (float(m.group(1)), float(m.group(2)), float(m.group(3) or 0)) if m else None

    def points(self):
        return [(float(a), float(b)) for a, b in re.findall(r"\(xy ([-\d.]+) ([-\d.]+)\)", self)]

    def label(self):
        m = re.match(r'\((?:global_|hierarchical_)?label "([^"]+)"', self)
        return m.group(1) if m else None

    def uuid(self):
        m = re.search(r'\(uuid "([^"]+)"\)', self)
        return m.group(1) if m else None


class Schematic:
    def __init__(self, head, items, tail, path=None):
        self.head, self.items, self.tail, self.path = head, [Item(i) for i in items], tail, path

    @classmethod
    def load(cls, path):
        head, items, tail = split_items(open(path, encoding="utf-8").read())
        return cls(head, items, tail, path)

    def save(self, path=None):
        body = "".join("\t" + item + "\n" for item in self.items)
        open(path or self.path, "w", encoding="utf-8").write(self.head + body + self.tail)

    @property
    def uuid(self):
        """Sheet uuid. version, generator, uuid, paper and title_block are top-level items too."""
        return re.search(r'\(uuid "([^"]+)"\)', next(i for i in self.items if i.kind() == "uuid")).group(1)

    def set_title(self, title):
        k = next(n for n, i in enumerate(self.items) if i.kind() == "title_block")
        self.items[k] = Item(re.sub(r'\(title "[^"]*"\)', '(title "%s")' % title, self.items[k]))

    def lib_symbols(self):
        return next(i for i in self.items if i.kind() == "lib_symbols")

    def has_lib_symbol(self, lib_id):
        return '(symbol "%s"' % lib_id in self.lib_symbols()

    def add_lib_symbol(self, block):
        """Embed a library symbol block, already renamed to its lib_id, into lib_symbols."""
        ls = self.lib_symbols()
        k = self.items.index(ls)
        self.items[k] = Item(ls[:-1].rstrip("\n") + "\n\t\t" + block.strip() + "\n\t)")

    def drop_lib_symbol(self, lib_id):
        blocks = [b for b in children(self.lib_symbols()) if not b.startswith('(symbol "%s"' % lib_id)]
        k = self.items.index(self.lib_symbols())
        self.items[k] = Item("(lib_symbols\n" + "".join("\t\t" + b + "\n" for b in blocks) + "\t)")

    def remove(self, pred):
        gone = [i for i in self.items if pred(i)]
        self.items = [i for i in self.items if not pred(i)]
        return gone

    def add(self, item):
        # keep sheet_instances last
        k = next((n for n, i in enumerate(self.items) if i.kind() == "sheet_instances"), len(self.items))
        self.items.insert(k, Item(item.strip()))

    def symbols(self):
        return [i for i in self.items if i.kind() == "symbol"]

    def by_ref(self, ref):
        return next((i for i in self.symbols() if i.ref() == ref), None)

    def wires_touching(self, x, y):
        return [i for i in self.items if i.kind() == "wire" and (x, y) in i.points()]

    def sheet_path(self):
        """Project path of this sheet for symbol instances, from any existing instance."""
        for i in self.symbols():
            m = re.search(r'\(path "([^"]+)"', i)
            if m:
                return m.group(1)
        return "/" + self.uuid


def children(block):
    """Direct child blocks of one item, as text (for example the symbols inside lib_symbols)."""
    depth, start, out, in_str = 0, None, [], False
    i = block.index("(") + 1
    while i < len(block):
        c = block[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            if depth == 0:
                start = i
            depth += 1
        elif c == ")":
            if depth == 0:
                break
            depth -= 1
            if depth == 0:
                out.append(block[start:i + 1])
        i += 1
    return out


def lib_symbol(lib_file, name, lib_id):
    """A library symbol block from a .kicad_sym file, renamed to lib_id for embedding."""
    text = open(lib_file, encoding="utf-8").read()
    start = text.index('\n\t(symbol "%s"' % name) + 2
    _, items, _ = split_items("(x\n" + text[start - 1:] + "\n)")
    block = items[0]
    block = block.replace('(symbol "%s"' % name, '(symbol "%s"' % lib_id, 1)
    return re.sub(r"\n\t", "\n\t\t", block)


def lib_symbol_pins(block):
    """[(number, name, x, y, rot)] from a library symbol block."""
    out = []
    for m in re.finditer(r'\(pin \w+ \w+\s*\(at ([-\d.]+) ([-\d.]+) (\d+)\)\s*\(length ([\d.]+)\)(.*?)\(number "([^"]+)"',
                         block, re.S):
        name = re.search(r'\(name "([^"]*)"', m.group(5)).group(1)
        out.append((m.group(6), name, float(m.group(1)), float(m.group(2)), int(m.group(3)), float(m.group(4))))
    return out


def _effects(size=1.27, justify=None, hide=False):
    j = "\n\t\t\t\t(justify %s)" % justify if justify else ""
    return "(effects\n\t\t\t\t(font\n\t\t\t\t\t(size %g %g)\n\t\t\t\t)%s\n\t\t\t)" % (size, size, j)


def _property(name, value, x, y, rot=0, hide=False, justify=None):
    return ('(property "%s" "%s"\n\t\t\t(at %g %g %g)\n%s\t\t\t(show_name no)\n\t\t\t(do_not_autoplace no)\n\t\t\t%s\n\t\t)'
            % (name, value, x, y, rot, "\t\t\t(hide yes)\n" if hide else "", _effects(justify=justify)))


def symbol_instance(lib_id, ref, value, footprint, x, y, rot=0, mirror=None, pins=(), sheet_path="/",
                    lcsc="", description="", datasheet="", dnp=False, ref_at=None, value_at=None,
                    in_bom=True, on_board=True, extra=()):
    """A symbol instance block. pins: iterable of pin numbers (each needs a uuid entry).
    ref_at / value_at: (dx, dy, justify) offsets of the visible Reference and Value fields."""
    rx, ry, rj = ref_at or (2.54, -1.27, "left")
    vx, vy, vj = value_at or (2.54, 1.27, "left")
    props = [_property("Reference", ref, x + rx, y + ry, 0, justify=rj),
             _property("Value", value, x + vx, y + vy, 0, justify=vj),
             _property("Footprint", footprint, x, y, hide=True),
             _property("Datasheet", datasheet, x, y, hide=True),
             _property("Description", description, x, y, hide=True)]
    if lcsc:
        props.append(_property("LCSC", lcsc, x, y, hide=True))
    for name, val in extra:
        props.append(_property(name, val, x, y, hide=True))
    pin_blocks = "".join('\n\t\t(pin "%s"\n\t\t\t(uuid "%s")\n\t\t)' % (p, new_uuid()) for p in pins)
    return ("(symbol\n\t\t(lib_id \"%s\")\n\t\t(at %g %g %g)%s\n\t\t(unit 1)\n\t\t(body_style 1)\n\t\t(exclude_from_sim no)\n"
            "\t\t(in_bom %s)\n\t\t(on_board %s)\n\t\t(in_pos_files yes)\n\t\t(dnp %s)\n\t\t(uuid \"%s\")\n\t\t%s%s\n"
            "\t\t(instances\n\t\t\t(project \"carrier\"\n\t\t\t\t(path \"%s\"\n\t\t\t\t\t(reference \"%s\")\n\t\t\t\t\t(unit 1)\n"
            "\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)"
            % (lib_id, x, y, rot, "\n\t\t(mirror %s)" % mirror if mirror else "", "yes" if in_bom else "no",
               "yes" if on_board else "no", "yes" if dnp else "no", new_uuid(), "\n\t\t".join(props), pin_blocks,
               sheet_path, ref))


def power_symbol(lib_id, value, x, y, rot=0, sheet_path="/", ref="#PWR0"):
    """GND or a project power flag. Value field visible, Reference hidden."""
    dy = 3.81 if value == "GND" else -3.81
    props = [_property("Reference", ref, x, y + (2.54 if value == "GND" else -2.54), 0, hide=True),
             _property("Value", value, x, y + dy, 0),
             _property("Footprint", "", x, y, hide=True), _property("Datasheet", "", x, y, hide=True),
             _property("Description", "Power symbol creates a global label with name \\\"%s\\\"" % value, x, y, hide=True)]
    return ("(symbol\n\t\t(lib_id \"%s\")\n\t\t(at %g %g %g)\n\t\t(unit 1)\n\t\t(body_style 1)\n\t\t(exclude_from_sim no)\n"
            "\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(in_pos_files yes)\n\t\t(dnp no)\n\t\t(uuid \"%s\")\n\t\t%s\n"
            "\t\t(pin \"1\"\n\t\t\t(uuid \"%s\")\n\t\t)\n"
            "\t\t(instances\n\t\t\t(project \"carrier\"\n\t\t\t\t(path \"%s\"\n\t\t\t\t\t(reference \"%s\")\n\t\t\t\t\t(unit 1)\n"
            "\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)"
            % (lib_id, x, y, rot, new_uuid(), "\n\t\t".join(props), new_uuid(), sheet_path, ref))


def global_label(name, x, y, rot=0, shape="bidirectional"):
    justify = {0: "left", 180: "right", 90: "left", 270: "right"}[rot % 360]
    return ('(global_label "%s"\n\t\t(shape %s)\n\t\t(at %g %g %g)\n\t\t(fields_autoplaced yes)\n\t\t%s\n\t\t(uuid "%s")\n'
            '\t\t(property "Intersheetrefs" "${INTERSHEET_REFS}"\n\t\t\t(at %g %g 0)\n\t\t\t(hide yes)\n\t\t\t(show_name no)\n'
            '\t\t\t(do_not_autoplace no)\n\t\t\t%s\n\t\t)\n\t)'
            % (name, shape, x, y, rot, _effects(justify=justify), new_uuid(), x, y, _effects(justify=justify)))


def wire(x1, y1, x2, y2):
    return ('(wire\n\t\t(pts\n\t\t\t(xy %g %g) (xy %g %g)\n\t\t)\n\t\t(stroke\n\t\t\t(width 0)\n\t\t\t(type default)\n\t\t)\n'
            '\t\t(uuid "%s")\n\t)' % (x1, y1, x2, y2, new_uuid()))


def junction(x, y):
    return '(junction\n\t\t(at %g %g)\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)\n\t\t(uuid "%s")\n\t)' % (x, y, new_uuid())


def no_connect(x, y):
    return '(no_connect\n\t\t(at %g %g)\n\t\t(uuid "%s")\n\t)' % (x, y, new_uuid())


def text(s, x, y, size=1.27):
    return ('(text "%s"\n\t\t(exclude_from_sim no)\n\t\t(at %g %g 0)\n\t\t%s\n\t\t(uuid "%s")\n\t)'
            % (s.replace('"', '\\"'), x, y, _effects(size=size, justify="left"), new_uuid()))


def rotate(dx, dy, rot, mirror=None):
    """Symbol-local pin offset to sheet offset. KiCad symbol y grows upward, sheet y downward."""
    if mirror == "x":
        dy = -dy
    if mirror == "y":
        dx = -dx
    for _ in range((int(round(rot)) // 90) % 4):
        dx, dy = -dy, dx
    return dx, -dy


def pin_positions(block, x, y, rot=0, mirror=None):
    """{number: (name, sheet_x, sheet_y, direction)} of a placed symbol; direction is the unit
    vector from pin end away from the body, for wiring out."""
    out = {}
    for num, name, px, py, prot, length in lib_symbol_pins(block):
        ex, ey = rotate(px, py, rot, mirror)
        # pin end is at (px, py); the body side is length further along prot
        bx, by = rotate(px + length * {0: 1, 90: 0, 180: -1, 270: 0}[prot],
                        py + length * {0: 0, 90: 1, 180: 0, 270: -1}[prot], rot, mirror)
        dx, dy = (ex - bx), (ey - by)
        norm = (dx * dx + dy * dy) ** 0.5 or 1.0
        out[num] = (name, round(x + ex, 4), round(y + ey, 4), (round(dx / norm), round(dy / norm)))
    return out


def stock_lib(name):
    return os.path.join("/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols", name + ".kicad_sym")
