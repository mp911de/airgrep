"""Shared plumbing for the simulation runners: ngspice batch runs with .param overrides,
waveform helpers and the Markdown report. Only the standard library is used."""

import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

NGSPICE = shutil.which("ngspice")


def require_ngspice():
    if not NGSPICE:
        sys.exit("ngspice not found on the PATH (brew install ngspice)")
    version = subprocess.run([NGSPICE, "--version"], capture_output=True, text=True).stdout
    found = re.search(r"ngspice-\S+", version)
    return found.group(0) if found else "ngspice"


def flatten(path):
    """Deck text with .include files inlined, so parameters can be set in one place."""
    out = []
    with open(path) as deck:
        for line in deck:
            include = re.match(r"\s*\.include\s+(\S+)", line, re.I)
            if include:
                out.append(flatten(os.path.join(os.path.dirname(path), include.group(1))))
            else:
                out.append(line)
    return "".join(out)


class Bench:
    """A bench directory: run its decks with .param overrides and read their wrdata tables."""

    def __init__(self, directory):
        self.directory = directory
        self.decks = {}

    def simulate(self, bench, **params):
        """Run one deck; rows of [x, y1, y2, ...] from its wrdata file."""
        if bench not in self.decks:
            self.decks[bench] = flatten(os.path.join(self.directory, bench + ".cir"))
        deck = self.decks[bench]
        for name, value in params.items():
            deck, count = re.subn(r"(?im)^\.param\s+%s\s*=.*$" % re.escape(name),
                                  ".param %s=%r" % (name, value), deck)
            if count != 1:
                sys.exit("%s: parameter %s defined %d times, expected once" % (bench, name, count))
        with tempfile.TemporaryDirectory() as tmp:
            with open(os.path.join(tmp, bench + ".cir"), "w") as out:
                out.write(deck)
            proc = subprocess.run([NGSPICE, "-b", bench + ".cir"], cwd=tmp,
                                  capture_output=True, text=True)
            data = os.path.join(tmp, bench + ".txt")
            if proc.returncode != 0 or not os.path.exists(data):
                sys.exit("ngspice failed on %s %r\n%s\n%s"
                         % (bench, params, proc.stdout, proc.stderr))
            rows = []
            with open(data) as table:
                for line in table:
                    values = [float(v) for v in line.split()]
                    if values:
                        rows.append([values[0]] + values[1::2])
        return rows

    def simulate_all(self, bench, runs):
        """Run a deck once per parameter set, in parallel, keeping the order."""
        with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
            return list(pool.map(lambda params: self.simulate(bench, **params), runs))


def crossing(rows, col, level, after=None):
    """x of the first crossing of column col through level (linear interpolation)."""
    for a, b in zip(rows, rows[1:]):
        if after is not None and a[0] < after:
            continue
        if (a[col] - level) * (b[col] - level) <= 0 and a[col] != b[col]:
            return a[0] + (b[0] - a[0]) * (level - a[col]) / (b[col] - a[col])
    return None


def value_at(rows, x, col):
    for a, b in zip(rows, rows[1:]):
        if a[0] <= x <= b[0]:
            return a[col] + (b[col] - a[col]) * (x - a[0]) / (b[0] - a[0])
    return rows[-1][col]


def window(rows, start, end=None):
    return [row for row in rows if row[0] >= start and (end is None or row[0] <= end)]


def minus(text):
    return text.replace("-", "−")


def deg(t):
    return "none" if t is None else minus("%.1f" % t)


def pct(ratio, digits=1):
    text = "%.*f" % (digits, 100 * ratio)
    if float(text) == 0:
        text = text.lstrip("-")
    return minus(text) + " %"


class Report:
    def __init__(self):
        self.lines = []
        self.failed = 0

    def add(self, text=""):
        self.lines.append(text)

    def table(self, header, rows):
        self.add("| " + " | ".join(header) + " |")
        self.add("|" + "---|" * len(header))
        for row in rows:
            self.add("| " + " | ".join(row) + " |")
        self.add()

    def check(self, ok, text):
        self.failed += 0 if ok else 1
        self.add("- %s: %s" % ("PASS" if ok else "**FAIL**", text))

    def finish(self, path=None):
        self.add("Result: %s." % ("all checks pass" if not self.failed
                                  else "%d check(s) failed" % self.failed))
        text = "\n".join(self.lines) + "\n"
        print(text, end="")
        if path:
            with open(path, "w") as out:
                out.write(text)
        return 1 if self.failed else 0


class Figure:
    """Vector drawing written as SVG and as PDF, without a plotting library. Coordinates in
    points, origin top left. Colours are #rrggbb; PDF has no transparency, so pass blended colours."""

    # Helvetica advance widths per 1000 units, enough to place anchored labels
    WIDTH = dict.fromkeys("0123456789", 556) | dict.fromkeys(" .,:/()[]|", 278) | {"-": 333, "−": 584, "°": 400}

    def __init__(self, width, height):
        self.w, self.h, self.items = width, height, []

    def line(self, points, color, width=1.0, dash=None):
        self.items.append(("line", points, color, width, dash))

    def rect(self, x, y, w, h, fill=None, stroke=None):
        self.items.append(("rect", (x, y, w, h), fill, stroke))

    def circle(self, x, y, r, fill):
        self.items.append(("circle", (x, y, r), fill))

    def text(self, x, y, s, size=11, color="#000000", anchor="start", bold=False, rotate=False):
        self.items.append(("text", (x, y), s, size, color, anchor, bold, rotate))

    def text_width(self, s, size):
        return sum(self.WIDTH.get(c, 667 if c.isupper() else 520) for c in s) * size / 1000

    def svg(self, path):
        esc = lambda s: s.replace("&", "&amp;").replace("<", "&lt;")
        out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%g" height="%g" viewBox="0 0 %g %g" '
               'font-family="Helvetica, Arial, sans-serif">' % (self.w, self.h, self.w, self.h),
               '<rect width="100%" height="100%" fill="#ffffff"/>']
        for kind, geo, *a in self.items:
            if kind == "line":
                color, width, dash = a
                out.append('<polyline fill="none" stroke="%s" stroke-width="%g"%s points="%s"/>'
                           % (color, width, ' stroke-dasharray="%g,%g"' % dash if dash else "",
                              " ".join("%.2f,%.2f" % p for p in geo)))
            elif kind == "rect":
                fill, stroke = a
                out.append('<rect x="%.2f" y="%.2f" width="%.2f" height="%.2f" fill="%s" stroke="%s"/>'
                           % (*geo, fill or "none", stroke or "none"))
            elif kind == "circle":
                out.append('<circle cx="%.2f" cy="%.2f" r="%g" fill="%s"/>' % (*geo, a[0]))
            else:
                s, size, color, anchor, bold, rotate = a
                out.append('<text x="%.2f" y="%.2f" font-size="%g" fill="%s" text-anchor="%s"%s%s>%s</text>'
                           % (*geo, size, color, {"start": "start", "middle": "middle", "end": "end"}[anchor],
                              ' font-weight="bold"' if bold else "",
                              ' transform="rotate(-90 %.2f %.2f)"' % geo if rotate else "", esc(s)))
        out.append("</svg>")
        with open(path, "w") as f:
            f.write("\n".join(out) + "\n")

    def pdf(self, path):
        rgb = lambda c: " ".join("%.3f" % (int(c[k:k + 2], 16) / 255) for k in (1, 3, 5))
        y = lambda v: self.h - v
        ops = []
        for kind, geo, *a in self.items:
            if kind == "line":
                color, width, dash = a
                ops.append("%s RG %g w [%s] 0 d" % (rgb(color), width, "%g %g" % dash if dash else ""))
                ops.append(" ".join("%.2f %.2f %s" % (px, y(py), "m" if k == 0 else "l")
                                    for k, (px, py) in enumerate(geo)) + " S")
            elif kind == "rect":
                (rx, ry, rw, rh), (fill, stroke) = geo, a
                paint = "B" if fill and stroke else "f" if fill else "S"
                ops.append("%s%s[] 0 d 1 w %.2f %.2f %.2f %.2f re %s" % (
                    "%s rg " % rgb(fill) if fill else "", "%s RG " % rgb(stroke) if stroke else "",
                    rx, y(ry + rh), rw, rh, paint))
            elif kind == "circle":
                (cx, cy, r), k = geo, 0.5523 * geo[2]
                cy = y(cy)
                ops.append("%s rg %.2f %.2f m %.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c "
                           "%.2f %.2f %.2f %.2f %.2f %.2f c %.2f %.2f %.2f %.2f %.2f %.2f c f"
                           % (rgb(a[0]), cx + r, cy, cx + r, cy + k, cx + k, cy + r, cx, cy + r,
                              cx - k, cy + r, cx - r, cy + k, cx - r, cy, cx - r, cy - k, cx - k, cy - r,
                              cx, cy - r, cx + k, cy - r, cx + r, cy - k, cx + r, cy))
            else:
                s, size, color, anchor, bold, rotate = a
                shift = {"start": 0, "middle": 0.5, "end": 1}[anchor] * self.text_width(s, size)
                tx, ty = geo
                body = s.replace("−", "-").encode("cp1252", "replace")
                body = body.replace(b"\\", b"\\\\").replace(b"(", b"\\(").replace(b")", b"\\)")
                matrix = "0 1 -1 0 %.2f %.2f" % (tx, y(ty) - shift) if rotate else \
                    "1 0 0 1 %.2f %.2f" % (tx - shift, y(ty))
                ops.append(b"BT /%s %g Tf %s rg %s Tm (" % (b"F2" if bold else b"F1", size, rgb(color).encode(),
                                                           matrix.encode()) + body + b") Tj ET")
        content = b"\n".join(o if isinstance(o, bytes) else o.encode() for o in ops)
        font = "<< /Type /Font /Subtype /Type1 /BaseFont /%s /Encoding /WinAnsiEncoding >>"
        objects = [b"<< /Type /Catalog /Pages 2 0 R >>",
                   b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
                   ("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %g %g] /Contents 4 0 R "
                    "/Resources << /Font << /F1 5 0 R /F2 6 0 R >> >> >>" % (self.w, self.h)).encode(),
                   b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
                   (font % "Helvetica").encode(), (font % "Helvetica-Bold").encode()]
        out, offsets = bytearray(b"%PDF-1.4\n"), []
        for n, body in enumerate(objects, 1):
            offsets.append(len(out))
            out += b"%d 0 obj\n" % n + body + b"\nendobj\n"
        xref = len(out)
        out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1)
        out += b"".join(b"%010d 00000 n \n" % o for o in offsets)
        out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objects) + 1, xref)
        with open(path, "wb") as f:
            f.write(out)
