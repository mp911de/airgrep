#!/usr/bin/env python3
"""Design consistency checks behind tools/check.sh. Reads only, writes nothing.

Run with KiCad's bundled Python (pcbnew, Python 3.9). tools/check.sh passes the
repo root and a temp directory holding erc.json, drc.json and sch.net from kicad-cli.
Prints one summary line per passing step and one line per finding. Exit code 1 on
any finding. Lines start with ok, FAIL or warn. Warnings do not fail the check.

Steps: erc, drc, nets, parity, bindings, models, and exports with --exports.
The binding table BINDINGS below ties schematic values, specs/power_params.json
and the constants in hardware/sim/run_*.py together. Extend it when a sim script
gains a constant that mirrors a part value.
"""
import ast, csv, json, operator, os, re, sys
import pcbnew

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import project  # noqa: E402

ROOT, TMP = sys.argv[1], sys.argv[2]
EXPORTS = "--exports" in sys.argv[3:]
KICAD = project.resolve()  # CARRIER_PROJECT, set by check.sh
CHARGER = project.charger(KICAD)
SIM = os.path.join(ROOT, "hardware/sim")
findings = 0


def fail(step, text):
    global findings
    findings += 1
    print("FAIL %-8s %s" % (step, text))


def ok(step, text):
    print("ok   %-8s %s" % (step, text))


def warn(step, text):
    print("warn %-8s %s" % (step, text))


# --- ERC and DRC reports from kicad-cli, coordinates in mil
def report_lines(step, kind, entries, where=""):
    """One line per error. Warnings print but do not fail. Returns (errors, warnings)."""
    errors = warnings = 0
    for v in entries:
        if v.get("excluded"):
            continue
        items = v.get("items", [])
        # A zone reports (0,0). Take the first item with a real position.
        pos = v.get("pos") or next((i["pos"] for i in items if i.get("pos") and (i["pos"]["x"] or i["pos"]["y"])), None)
        at = "(%d,%d) mil " % (round(pos["x"]), round(pos["y"])) if pos else ""
        text = "%s%s %s%s" % (where, kind or v.get("type", ""), at, v.get("description", ""))
        if items:
            text += ": " + " / ".join(i.get("description", "") for i in items)
        if v.get("severity") == "warning" and not kind:
            warnings += 1
            warn(step, text)
        elif v.get("severity") != "exclusion" or kind:
            errors += 1
            fail(step, text)
    return errors, warnings


def check_erc():
    d = json.load(open(os.path.join(TMP, "erc.json")))
    e = w = 0
    for sheet in d.get("sheets", []):
        se, sw = report_lines("erc", "", sheet.get("violations", []), sheet.get("path", "") + " ")
        e, w = e + se, w + sw
    if not e:
        ok("erc", "0 errors, %d warnings" % w)


def check_drc():
    d = json.load(open(os.path.join(TMP, "drc.json")))
    e, w = report_lines("drc", "", d.get("violations", []))
    u, _ = report_lines("drc", "unconnected", d.get("unconnected_items", []))
    p, _ = report_lines("drc", "parity", d.get("schematic_parity", []))
    if not e + u + p:
        ok("drc", "0 errors, %d warnings, 0 unconnected, 0 parity issues, zones refilled in memory" % w)


# --- schematic netlist (kicad-cli sch export netlist, s-expression)
def sexpr(text):
    stack, cur = [], []
    for tok in re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+', text):
        if tok == "(":
            stack.append(cur)
            cur = []
        elif tok == ")":
            done, cur = cur, stack.pop()
            cur.append(done)
        else:
            cur.append(tok[1:-1].replace('\\"', '"') if tok.startswith('"') else tok)
    return cur[0]


def child(node, name):
    return next((c for c in node if isinstance(c, list) and c and c[0] == name), None)


def children(node, name):
    return [c for c in node if isinstance(c, list) and c and c[0] == name]


def text_of(node, name):
    c = child(node, name)
    return c[1] if c and len(c) > 1 else ""


def load_schematic():
    """Returns ({ref: dict(value, footprint, lcsc, dnp)}, [net names])."""
    tree = sexpr(open(os.path.join(TMP, "sch.net"), encoding="utf-8").read())
    comps = {}
    for c in children(child(tree, "components"), "comp"):
        props = {text_of(p, "name") for p in children(c, "property")}
        if "exclude_from_board" in props:
            continue
        fields = {text_of(f, "name"): (f[2] if len(f) > 2 else "") for f in children(child(c, "fields") or [], "field")}
        comps[text_of(c, "ref")] = {"value": text_of(c, "value"), "footprint": text_of(c, "footprint"),
                                    "lcsc": fields.get("LCSC", ""), "dnp": "dnp" in props,
                                    "tolerance": fields.get("Tolerance", ""), "tcr": fields.get("TCR", "")}
    return comps, [text_of(n, "name") for n in children(child(tree, "nets"), "net")]


def load_board():
    """Returns (board, {ref: dict(...)}) without the board-only footprints."""
    board = pcbnew.LoadBoard(os.path.join(KICAD, "carrier.kicad_pcb"))
    fps = {}
    for f in board.GetFootprints():
        if f.GetAttributes() & pcbnew.FP_BOARD_ONLY:
            continue
        fps[f.GetReference()] = {"value": f.GetValue(), "footprint": f.GetFPIDAsString(),
                                 "lcsc": f.GetFieldText("LCSC") if f.HasField("LCSC") else "",
                                 "dnp": bool(f.IsDNP()), "fp": f}
    return board, fps


# --- every net carries a label name, never an auto name
def check_nets(board, sch_nets):
    bad = 0
    pads = {}
    for pad in board.GetPads():
        pads.setdefault(pad.GetNetname(), []).append(pad)
    board_nets = [str(n) for n in board.GetNetsByName().keys() if str(n)]
    for name in sorted(n for n in board_nets if n.startswith("Net-(")):
        p = pads.get(name, [])
        at = " at (%d,%d) mil" % (round(pcbnew.ToMils(p[0].GetPosition().x)),
                                  round(pcbnew.ToMils(p[0].GetPosition().y))) if p else ""
        fail("nets", "board net %s has no label name, %d pads%s" % (name, len(p), at))
        bad += 1
    for name in sorted(n for n in sch_nets if n.startswith("Net-(")):
        fail("nets", "schematic net %s has no label name" % name)
        bad += 1
    if not bad:
        ok("nets", "%d board nets, %d schematic nets, no auto-named Net-(...) name" % (len(board_nets), len(sch_nets)))


# --- schematic against board, per reference
NAMES = {"lcsc": "LCSC code", "dnp": "DNP"}


def check_parity(sch, brd):
    bad = 0
    for ref in sorted(set(sch) | set(brd)):
        if ref not in brd or ref not in sch:
            fail("parity", "%s is missing %s" % (ref, "on the board" if ref in sch else "in the schematic"))
            bad += 1
            continue
        for key in ("value", "footprint", "lcsc", "dnp"):
            if sch[ref][key] != brd[ref][key]:
                fail("parity", "%s %s: schematic %r, board %r" % (ref, NAMES.get(key, key), sch[ref][key], brd[ref][key]))
                bad += 1
    if not bad:
        ok("parity", "%d references agree in value, footprint, LCSC code and DNP" % len(sch))


# --- bindings between schematic, power_params.json and the sim constants
SI = {"p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6, "m": 1e-3, "": 1.0, "R": 1.0, "k": 1e3, "M": 1e6}


def part_value(text):
    """'154k 1%' gives 154e3, '2.2uH CKST0603' gives 2.2e-6, '4k7' gives 4700."""
    tok = text.split()[0]
    m = re.match(r"^(\d+)([kMR])(\d+)$", tok)
    if m:
        return float(m.group(1) + "." + m.group(3)) * SI[m.group(2)]
    m = re.match(r"^(\d+(?:\.\d+)?)([pnuµmkMR]?)[HFΩ]?$", tok)
    if not m:
        raise ValueError("cannot read part value %r" % text)
    return float(m.group(1)) * SI[m.group(2)]


OPS = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul, ast.Div: operator.truediv}
_sim = {}


def sim_env(name):
    """Top-level constants of a sim script, evaluated from the syntax tree without importing it."""
    if name in _sim:
        return _sim[name]
    env = _sim[name] = {}

    def ev(n):
        if isinstance(n, ast.Constant) and isinstance(n.value, (int, float)):
            return n.value
        if isinstance(n, ast.Tuple):
            return tuple(ev(e) for e in n.elts)
        if isinstance(n, ast.BinOp) and type(n.op) in OPS:
            return OPS[type(n.op)](ev(n.left), ev(n.right))
        if isinstance(n, ast.UnaryOp) and isinstance(n.op, ast.USub):
            return -ev(n.operand)
        if isinstance(n, ast.Name):
            return env[n.id]
        if isinstance(n, ast.Attribute) and isinstance(n.value, ast.Name) and n.value.id.startswith("run_"):
            return sim_env(n.value.id)[n.attr]
        raise KeyError("not a constant")

    for node in ast.parse(open(os.path.join(SIM, name + ".py"), encoding="utf-8").read()).body:
        if not isinstance(node, ast.Assign) or len(node.targets) != 1:
            continue
        try:
            value, target = ev(node.value), node.targets[0]
        except (KeyError, TypeError, ZeroDivisionError):
            continue
        if isinstance(target, ast.Name):
            env[target.id] = value
        elif isinstance(target, ast.Tuple) and isinstance(value, tuple):
            env.update({t.id: v for t, v in zip(target.elts, value) if isinstance(t, ast.Name)})
    return env


# Terms of a binding. Each returns (label, function of the context returning a number).
def sch(*refs):
    return "schematic " + "+".join(refs), lambda c: sum(part_value(c["sch"][r]["value"]) for r in refs)


def sch_field(ref, field, scale=1.0):
    return "schematic %s %s" % (ref, field), lambda c: float(c["sch"][ref][field].split()[0].rstrip("%")) * scale


def sim(script, name, index=None, scale=1.0, digits=None):
    def get(c):
        v = sim_env(script)[name]
        for i in (index if isinstance(index, tuple) else () if index is None else (index,)):
            v = v[i]
        v *= scale
        return round(v, digits) if digits is not None else v
    path = "" if index is None else "".join("[%d]" % i for i in (index if isinstance(index, tuple) else (index,)))
    return "%s.py %s%s" % (script, name, path), get


def par(path, scale=1.0):
    """Number in specs/power_params.json. Path items are keys, list indexes or name prefixes."""
    def get(c):
        v = c["params"]
        for key in path.split("/"):
            if isinstance(v, list):
                v = v[int(key)] if key.isdigit() else next(e for e in v if e["name"].startswith(key))
            else:
                v = v[key]
        return v * scale
    return "power_params " + path, get


def prose(src, pattern, scale=1.0, group=1):
    """Number quoted in prose. src is a power_params path to a string or a sim script name."""
    def get(c):
        if src.startswith("run_") or src == "chargers":
            text = open(os.path.join(SIM, src + ".py"), encoding="utf-8").read()
        else:
            text = par(src, 1)[1](c) if not src.endswith("#keys") else " ".join(c["params"][src[:-5]])
        m = re.search(pattern, text)
        if not m:
            raise LookupError("text /%s/ not found" % pattern)
        return float(m.group(group)) * scale
    return "%s text /%s/" % (src, pattern), get


def lit(value, note):
    return "%g (%s)" % (value, note), lambda c: value


def chg(*keys):
    """A value of the project's charger entry in hardware/sim/chargers.py, nested keys allowed."""
    sys.path.insert(0, SIM)
    import chargers

    def get(c):
        v = chargers.CHARGERS[CHARGER]
        for k in keys:
            v = v[k]
        return v
    return "chargers.py %s/%s" % (CHARGER, "/".join(keys)), get


def bst(*keys):
    """A value of the production boost entry in hardware/sim/boosts.py."""
    sys.path.insert(0, SIM)
    import boosts

    def get(c):
        v = boosts.PROFILES[boosts.DEFAULT]
        for k in keys:
            v = v[k]
        return v
    return "boosts.py %s/%s" % (boosts.DEFAULT, "/".join(keys)), get


CHG = "charging/_charge_current_a"
IFAST = r"R6 \+ R37 = ([\d.]+) kΩ \+ ([\d.]+) kΩ in series = ([\d.]+) kΩ"
BINDINGS = [
    # boost converter and load switch
    (sch("R14"), sim("run_power", "R14")),
    (sch("R15"), sim("run_power", "R15")),
    (sch("R16"), sim("run_power", "R16")),
    (sch("R17"), sim("run_power", "R17")),
    (sch("R18"), sim("run_power", "R18")),
    (sch("C21"), sim("run_power", "C21")),
    (sch("C22"), sim("run_power", "C22")),
    (sch("C17"), sim("run_power", "C17")),
    (sch("C23"), sim("run_power", "C23_CT")),
    (sch("C24"), sim("run_power", "C24")),
    (sch("C38"), sim("run_power", "C_INPUT")),
    (sch("C39"), sim("run_power", "C_INPUT")),
    (sim("run_power", "ID_RON", 0), sim("run_heat", "ID_RON", 0)),
    (sim("run_power", "ID_RON", 1), sim("run_heat", "ID_RON", 1)),
    (sim("run_power", "ID_VFWD", 0), sim("run_heat", "ID_VFWD", 0)),
    (sim("run_power", "ID_VFWD", 1), sim("run_heat", "ID_VFWD", 1)),
    (sch("C18", "C19", "C20"), sim("run_power", "C_MAIN")),
    (sch("L2"), sim("run_power", "L2")),
    (sch("L1"), chg("l1", "l")),
    (sch("R14"), prose("_rail_5v_v", r"R14/R15 \((\d+) kΩ / (\d+) kΩ\)", 1e3)),
    (sch("R15"), prose("_rail_5v_v", r"R14/R15 \((\d+) kΩ / (\d+) kΩ\)", 1e3, 2)),
    (sch("R16"), prose("_boost_efficiency_curve", r"R16 (\d+) kΩ", 1e3)),
    (par("rail_5v_v"), sim("run_power", "VOUT", digits=2)),
    (par("load_switch_ron_ohm"), bst("load_switch_ron")),
    (sim("run_power", "L2_DCR"), prose("_boost_efficiency_curve", r"DCR \(([\d.]+) mΩ\)", 1e-3)),
    # loads, fuse, pack
    (sim("run_heat", "LED_5V_A", scale=1e3), par("loads_5v/Carrier: green power LED D5/ma")),
    (sch("R26"), prose("loads_5v/Carrier: green power LED D5/source", r"/ (\d+) Ω \(R26\)")),
    (sch("R10"), prose("_battery_side_quiescent_ma", r"R10 (\d+) MΩ", 1e6)),
    (sch("R10"), par("off_state/common/R10/ohm")),
    (sim("run_heat", "FUSE_R", 0), prose("_path_resistance_ohm", r"fuse F1 [^(]*\((\d+) mΩ nominal\)", 1e-3)),
    (sim("run_heat", "PACK_MM", 0), prose("batteries/1/dimensions_mm", r"^(\d+) x (\d+) x (\d+)")),
    (sim("run_heat", "PACK_MM", 1), prose("batteries/1/dimensions_mm", r"^(\d+) x (\d+) x (\d+)", 1, 2)),
    (sim("run_heat", "PACK_MM", 2), prose("batteries/1/dimensions_mm", r"^(\d+) x (\d+) x (\d+)", 1, 3)),
]
# charger bindings per project; the MAX77757 pair 12.4 kΩ and 1.8 A is one row of its data sheet Table 3.
# A new charge current means new values in both literal rows.
BINDINGS_MAX = [
    (sch("R8"), sim("run_thermal", "R8")),  # thermal cut-off network
    (sch("R6", "R37"), lit(12.4e3, "IFAST resistor for 1.8 A")),
    (chg("charge_a"), lit(1.8, "IFAST current for 12.4 kΩ")),
    (chg("charge_a"), par("charging/charge_current_a")),
    (par("charging/charge_current_a"), prose(CHG, r"gives ([\d.]+) A nominal")),
    (sch("R6"), prose(CHG, IFAST, 1e3)),
    (sch("R37"), prose(CHG, IFAST, 1e3, 2)),
    (sch("R6", "R37"), prose(CHG, IFAST, 1e3, 3)),
    (sch("R6", "R37"), prose("chargers", r'charge_note="R6 \+ R37 = ([\d.]+) kΩ', 1e3)),
    (chg("r_batfet"), prose("_path_resistance_ohm", r"BATT-to-SYS switch \((\d+) mΩ typ\)", 1e-3)),
]
BINDINGS_TI = [
    (sch("R73"), sim("run_power", "OVLO_RAIL", (0, 0))),
    (sch("R74"), sim("run_power", "OVLO_RAIL", (1, 0))),
    (sch("R75"), sim("run_power", "OVLO_RAIL", (0, 0))),
    (sch("R76"), sim("run_power", "OVLO_RAIL", (1, 0))),
    (sch("R77"), sim("run_power", "OVLO_USB", (0, 0))),
    (sch("R78"), sim("run_power", "OVLO_USB", (1, 0))),
    (sch("R56"), lit(4.7e3, "PROG: 1S, 750 kHz, BQ25798 Table 7-1")),
    (sch("R57"), lit(120e3, "ILIM_HIZ divider top, 2.2 V from REGN 4.8 V for a 1.5 A clamp")),
    (sch("R58"), lit(100e3, "ILIM_HIZ divider bottom")),
    (sch("R59"), lit(80.6e3, "TS divider RT1 for the 100k B4262 NTC, window 3 to 43 C")),
    (sch("R60"), lit(768e3, "TS divider RT2")),
    (sch("R19"), lit(1.43e3, "I2C SDA pull-up for the 200 pF Fast-mode envelope with either ESP module")),
    (sch("R20"), lit(1.43e3, "I2C SCL pull-up for the 200 pF Fast-mode envelope with either ESP module")),
    (sch("R79"), lit(1.5e3, "FUSB303B VBUS_DET series resistor; the pin loads to GND with 72.4 kOhm min, so it stays small")),
    (sch("R80"), lit(768e3, "FUSB303B PORT strap below the 1 to 4 MOhm float band; same part as R60")),
    (sch("R81"), lit(768e3, "FUSB303B ADDR strap below the 1 to 4 MOhm float band; same part as R60")),
    (chg("charge_a"), par("charging/charge_current_a")),
    (chg("fsw"), lit(750e3, "PROG 4.7 kΩ")),
]
for ref, divider, position in (("R73", "OVLO_RAIL", 0), ("R74", "OVLO_RAIL", 1), ("R75", "OVLO_RAIL", 0),
                               ("R76", "OVLO_RAIL", 1), ("R77", "OVLO_USB", 0), ("R78", "OVLO_USB", 1)):
    BINDINGS_TI.extend([(sch_field(ref, "tolerance", .01), sim("run_power", divider, (position, 1))),
                        (sch_field(ref, "tcr", 1e-6), sim("run_power", divider, (position, 2)))])
BINDINGS += {"max": BINDINGS_MAX, "ti": BINDINGS_TI}[CHARGER]


def check_bindings(sch_comps):
    ctx = {"sch": sch_comps, "params": json.load(open(os.path.join(ROOT, "specs/power_params.json"), encoding="utf-8"))}
    bad = 0
    for (la, fa), (lb, fb) in BINDINGS:
        try:
            a, b = fa(ctx), fb(ctx)
        except Exception as e:  # a missing reference, constant or text is a finding too
            fail("bindings", "%s = %s: cannot read, %s %s" % (la, lb, type(e).__name__, e))
            bad += 1
            continue
        if abs(a - b) > 1e-6 * max(abs(a), abs(b)):
            fail("bindings", "%s is %g, %s is %g" % (la, a, lb, b))
            bad += 1
    if not bad:
        ok("bindings", "%d bindings between schematic, power_params.json and sim constants agree" % len(BINDINGS))


# --- 3D models, warnings only
def check_models(brd):
    common = os.path.expanduser("~/Library/Preferences/kicad/10.0/kicad_common.json")
    env = {}
    if os.path.exists(common):
        env.update((json.load(open(common)).get("environment") or {}).get("vars") or {})
    env.update(os.environ)
    env["KIPRJMOD"] = KICAD
    stock = "/Applications/KiCad/KiCad.app/Contents/SharedSupport/3dmodels"
    home, n = os.path.expanduser("~"), 0
    no_body = pcbnew.FP_EXCLUDE_FROM_POS_FILES | pcbnew.FP_BOARD_ONLY | pcbnew.FP_DNP
    for ref in sorted(brd, key=lambda r: (re.sub(r"\d", "", r), int(re.sub(r"\D", "", r) or 0))):
        f = brd[ref]["fp"]
        models = [m.m_Filename for m in f.Models()]
        # Test points, fiducials, mounting holes and unfitted parts have no body by design.
        if not models and not f.GetAttributes() & no_body:
            warn("models", "%s has no 3D model" % ref)
            n += 1
        for raw in models:
            path = re.sub(r"\$\{(\w+)\}", lambda m: env.get(m.group(1), stock if "3DMODEL_DIR" in m.group(1) else m.group(0)), raw)
            if path.startswith("~") or (os.path.realpath(path).startswith(home + os.sep)
                                         and not os.path.realpath(path).startswith(os.path.realpath(ROOT) + os.sep)):
                warn("models", "%s model in a personal directory: %s" % (ref, raw))
                n += 1
            elif not os.path.isfile(path):
                warn("models", "%s model file missing: %s" % (ref, raw))
                n += 1
    if not n:
        ok("models", "all 3D model paths resolve")


# --- JLCPCB production files, only with --exports
def check_exports(brd):
    prod = os.path.join(KICAD, "jlcpcb/production_files")
    bom, cpl, bad = {}, set(), 0
    for row in csv.DictReader(open(os.path.join(prod, "BOM-carrier.csv"), encoding="utf-8-sig")):
        for ref in row["Designator"].split(","):
            bom[ref.strip()] = row.get("LCSC", "")
    for row in csv.DictReader(open(os.path.join(prod, "CPL-carrier.csv"), encoding="utf-8-sig")):
        cpl.add(row["Designator"].strip())
    for ref in sorted(set(brd) | set(bom) | cpl):
        f = brd.get(ref)
        attr = f["fp"].GetAttributes() if f else 0
        fitted = bool(f) and not f["dnp"]
        for name, listed, excluded in (("BOM", ref in bom, attr & pcbnew.FP_EXCLUDE_FROM_BOM),
                                       ("CPL", ref in cpl, attr & pcbnew.FP_EXCLUDE_FROM_POS_FILES)):
            if fitted and not excluded and not listed:
                fail("exports", "%s is fitted and missing in the %s" % (ref, name))
                bad += 1
            elif listed and not (fitted and not excluded):
                fail("exports", "%s is in the %s and %s" % (ref, name, "not on the board" if not f else "not fitted"))
                bad += 1
        if fitted and ref in bom and bom[ref] != f["lcsc"]:
            fail("exports", "%s LCSC code: BOM %r, board %r" % (ref, bom[ref], f["lcsc"]))
            bad += 1
    if not bad:
        ok("exports", "BOM and CPL list every fitted reference and no other")


def main():
    check_erc()
    check_drc()
    sch_comps, sch_nets = load_schematic()
    board, brd = load_board()
    check_nets(board, sch_nets)
    check_parity(sch_comps, brd)
    check_bindings(sch_comps)
    check_models(brd)
    if EXPORTS:
        check_exports(brd)
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
