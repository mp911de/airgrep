"""Boost-converter variants used by the heat and EMI estimates."""

DEFAULT = "tps61088"


def tps61088_fsw(vin):
    # TPS61088 data sheet frequency equation with R16 = 220 kOhm.
    return 1 / (220e3 * 23e-12 / 4 + 89e-9 * 5.057 / vin)


PROFILES = {
    "tps61088": {
        "part": "TPS61088",
        "description": "fitted carrier boost with TPS22975 output switch",
        "r_ls": 11e-3,
        "r_hs": 13e-3,
        "theta_board": 4.1,
        "theta_ja": 38.8,
        "fsw": tps61088_fsw,
        "fit_fsw": lambda vin: 1 / (150e3 * 23e-12 / 4 + 89e-9 * 5.057 / vin),
        "coss": 1e-9,
        "l2_l": 2.2e-6,
        "l2_dcr": 10.7e-3,
        "l2_tol": .20,
        "l2_isat_10": 7.3,
        "l2_label": "FC-ALX4030D-2R2MT (C5370906)",
        "emi_efficiency": .96,
        "spread_fraction": 0.0,
        "load_switch": True,
        "load_switch_ron": 16e-3,
        "fit_vin": 3.6,
        "efficiency_curve": None,
        "efficiency_note": "TI TPS61088 Figure 6-2 curve from specs/power_params.json",
        "layout_note": None,
        "datasheet": "SLVSCM8D",
    },
    "tps61023": {
        "part": "TPS61023",
        "description": "candidate integrated-disconnect boost, retaining L2 2.2 uH",
        "r_ls": 47e-3,
        "r_hs": 68e-3,
        "theta_board": 31.0,
        "theta_ja": 91.4,
        "fsw": lambda vin: 1e6,
        "fit_fsw": lambda vin: 1e6,
        # Neither switch capacitance nor edge time is specified. 300 pF represents this
        # 3 A class converter and is swept by 0.5x and 2x in run_emi.py.
        "coss": 300e-12,
        "l2_l": 2.2e-6,
        "l2_dcr": 10.7e-3,
        "l2_tol": .20,
        "l2_isat_10": 7.3,
        "l2_label": "FC-ALX4030D-2R2MT (C5370906)",
        "emi_efficiency": .90,
        "spread_fraction": 0.0,
        "load_switch": False,
        "load_switch_ron": 0.0,
        "fit_vin": 3.0,
        # Read from TPS61023 Figure 6-1, VIN 3.0 V, VOUT 5 V. The 3.0 V curve is a
        # conservative proxy for the carrier's 3.1 V low-battery operating point.
        "efficiency_curve": ((0.05, .932), (.10, .934), (.20, .938), (.50, .945),
                             (.80, .940), (1.00, .933), (1.50, .909), (2.00, .882)),
        "efficiency_note": "TI TPS61023 Figure 6-1, 3.0 V curve read to about 0.5 percentage point",
        "layout_note": ("Candidate silicon at the present U6 thermal location and on its hot-loop geometry. "
                        "U7 silicon loss is removed, but the present U6-to-U7 copper route remains. The real "
                        "TPS61023 layout does not exist; this isolates silicon effects from layout area."),
        "valley_limit_min": 2.7,
        "datasheet": "SLVSF14B",
    },
    "tps61378q1": {
        "part": "TPS61378-Q1",
        "description": "300 kHz forced-PWM integrated-disconnect candidate",
        "vout": 5.152,
        "r_ls": 50e-3,
        "r_hs": 50e-3,
        "isolation_ron": 100e-3,
        "theta_board": 18.5,
        "theta_ja": 46.2,
        "fsw": lambda vin: 300e3,
        "fsw_min": 270e3,
        "emi_fsw": 300e3,
        "emi_fsw_options": (270e3, 345e3),
        "fit_fsw": lambda vin: 2.2e6,
        # Switch capacitance and edge time are not specified. The 400 pF assumption is
        # swept by 0.5x and 2x in run_emi.py.
        "coss": 400e-12,
        "l2_l": 3.3e-6,
        "l2_dcr": 23.0e-3,
        "l2_tol": .20,
        "l2_isat_10": 9.5,
        "l2_label": "CENKER CKST0603-3.3uH/M (C3002635)",
        "emi_efficiency": .90,
        "spread_fraction": .10,
        "spread_rate_fraction": .004,
        "load_switch": False,
        "load_switch_ron": 0.0,
        "fit_vin": 3.3,
        # Read from TPS61378-Q1 Figure 6-2, VIN 3.3 V, VOUT 5 V, FPWM at 2.2 MHz.
        # Runtime switching loss is scaled to the proposed 300 kHz setting.
        "efficiency_curve": ((.05, .82), (.10, .88), (.20, .91), (.50, .925),
                             (.80, .92), (1.00, .91), (1.50, .87), (2.00, .84)),
        "efficiency_note": ("TI TPS61378-Q1 Figure 6-2, 3.3 V FPWM curve at 2.2 MHz "
                            "read to about one percentage point"),
        "layout_note": ("Candidate silicon at the present U6 thermal location. The internal 100 mOhm isolation "
                        "FET replaces U7. The proposed 300 kHz setting uses the stocked 3.3 uH CKST0603 variant and requires "
                        "a switch/output-capacitor loop no larger than 2.5 mm2. The real candidate layout does not exist."),
        "loop_area_model": 2.5e-6,
        "compensation": (33e3, 2.2e-9, 4.7e-12),
        "feedback": (408e3, 75e3),
        "peak_limit_min": 4.0,
        "datasheet": "SLVSET0E",
    },
}


def get(name):
    try:
        return PROFILES[name]
    except KeyError as exc:
        raise ValueError("unknown boost %s" % name) from exc
