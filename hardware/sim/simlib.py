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
