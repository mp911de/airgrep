"""Charger descriptions for the simulations: one entry per carrier project.

run_heat.py binds its CHG_* names from the entry that matches the project (tools/project.py).
Every number carries its source in a comment. Pad numbers are those of the board footprint.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import project  # noqa: E402

# Efficiency curves digitised from the data sheet figures (VBUS 5 V, VBAT 4 V, PFM disabled) as
# [charge current A, efficiency]. Read off a 130 dpi render of SLUSDV2C page 18, about ±0.5 point.
BQ25798_EFFICIENCY = {
    750e3: [[0.5, 0.93], [0.75, 0.948], [1.0, 0.952], [1.25, 0.953], [1.5, 0.95], [2.0, 0.945], [2.5, 0.938],
            [3.0, 0.93], [4.0, 0.915], [5.0, 0.90]],
    1.5e6: [[0.5, 0.92], [0.75, 0.935], [1.0, 0.94], [1.25, 0.944], [1.5, 0.942], [2.0, 0.935], [2.5, 0.928],
            [3.0, 0.92], [4.0, 0.90], [5.0, 0.88]],
}

CHARGERS = {
    "max": dict(
        part="MAX77757", ref="U4", datasheet="19-100986 Rev 2, Electrical Characteristics",
        pads=dict(vin=("23", "24"), sw=("20", "21"), sw2=None, sys=("15", "16"), bat=("13", "14"), gnd=("18", "19")),
        nets=dict(sw="CHG_SW", sw2=None),
        pin_names=dict(vin="CHGIN", sw="LX", sys="SYS", bat="BATT", gnd="PGND"),
        inductor_on_vsys=True,  # L1 output is the VSYS copper; the SYS pins sink the charge current
        r_in=45e-3, r_hs=60e-3, r_ls=60e-3, r_series=0.0, r_batfet=20e-3,  # RCHGIN2BYP, RHS, RLS, RBAT2SYS typ
        iq=2.7e-3,  # CHGIN supply current, not switching, typ
        fsw=1.3e6, vsysmin=3.5, treg=130.0, treg_options=(130.0,),
        theta_jc=7.5, theta_ja_jedec=31.0,  # JESD51-7 four-layer board; θJC surface not stated
        theta_note="θJC 7.5 K/W applied from junction to pins (surface not stated for the FC2QFN)",
        efficiency=None, sw_high=1.3,  # no efficiency curve: TPS61088 coefficient, high corner +30 %
        charge_a=1.8, charge_note="R6 + R37 = 12.4 kΩ, data sheet Table 3",
        losses=("MAX77757 buck", "MAX77757 BATT-SYS switch"),
        emi_loop=(("C6", "1"), ("U4", "22"), ("U4", "18"), ("C6", "2")),  # BYP capacitor, BYP pin, PGND pin
        emi_caps=("C6",),
        l1=dict(label="FXL0630-1R0-M", dcr=7.4e-3, l=1.0e-6),  # docs/power.md
        bulk_caps=("C1", "C5", "C6", "C7", "C9", "C10", "C11", "C13", "C14", "C15", "C18", "C19", "C20", "C24", "C25",
                   "C26", "C33", "C34"),  # X7R (LCSC C105174, C415535, C3039694, C385906, C907991)
        usb_cases=[
            dict(key="usb37", label="USB 3 A source, cell 3.7 V, charging, 0.8 A load", vcell=3.7, load=0.8),
            dict(key="usb32", label="USB 3 A source, cell 3.2 V, charging, 0.8 A load", vcell=3.2, load=0.8),
            dict(key="usb32off", label="USB 3 A source, cell 3.2 V, charging, device off", vcell=3.2, load=0.0),
        ],
        # 10 x 10 mm aluminium sink on the package top through a copper spacer and epoxy (see docs/power.md)
        heatsink={"U4": dict(type="top", label="10 x 10 mm aluminium sink (Fischer ICK S 10 x 10 x 6.5) on a 1.5 mm "
                                               "copper spacer, thermal epoxy", size_mm=10.0,
                             r_jt=35.0,  # K/W, junction to package top (estimate, 25 to 45)
                             r_tim=20.0,  # K/W, two 0.1 mm epoxy layers (k 1.2 W/m·K) on the 9 mm² top plus the spacer
                             r_sink_open=35.0)},  # K/W, sink to still air, Fischer figure derated for 30 to 40 K
        ts_hot=42.1, ts_restart=40.8,  # C, pack NTC hot stop and restart, thermal/results_max.md
        foldback_width=20.0,  # K: charge current reaches zero 20 K above TREG (5 % of IFC per K)
        default_a=None,  # no host, the resistor setting is the only current
        transient_policies=[("stock", "1.8 A + pack JEITA", 1.8, 130.0, None), ("low", "1.2 A + pack JEITA", 1.2, 130.0, None),
                            ("early", "90-100 °C target + JEITA", 1.8, 130.0, (90.0, 100.0))],
    ),
    "ti": dict(
        part="BQ25798", ref="U4", datasheet="SLUSDV2C, Electrical Characteristics and section 6.7",
        pads=dict(vin=("2", "3"), sw=("28",), sw2=("26",), sys=("25",), bat=("22", "23"), gnd=("27",)),
        nets=dict(sw="CHG_SW1", sw2="CHG_SW2"),
        board_nets={"USB_VBUS_J1": "+VUSB_J1", "USB_VBUS": "+VUSB", "VSYS": "+VSYS", "VBAT": "+VBAT",
                    "BOOST_5V": "+BOOST_5V", "RAIL_5V": "+5V", "ESP_5V_D": "+ESP_5V_D",
                    "ADS_5V_D": "+ADS_5V_D", "ESP_5V": "+ESP_5V", "ADS_5V": "+ADS_5V"},
        pin_names=dict(vin="VBUS", sw="SW1", sys="SYS", bat="BAT", gnd="GND"),
        inductor_on_vsys=False,  # L1 sits between SW1 and SW2; SYS is a separate pin behind Q3
        ship_fet=dict(ref="Q13", net="CHG_BAT", r_on=(2.6e-3, 6.4e-3)),  # CSD17575Q3: 4.5 V typical; high includes 2x hot derating
        r_in=8e-3, r_hs=24e-3, r_ls=35e-3, r_series=28e-3, r_batfet=11e-3,  # RSNS, Q1, Q2, Q3 (on in buck mode), BATFET typ
        iq=3e-3,  # VBUS supply current, converter switching, no load, typ
        fsw=750e3, vsysmin=3.5, treg=120.0, treg_options=(60.0, 80.0, 100.0, 120.0),
        theta_jc=9.7, theta_ja_jedec=44.2,  # RθJB 9.7 K/W junction to board, RθJA JEDEC four-layer
        theta_note="RθJB 9.7 K/W from junction to pins (RθJC(top) 20.9, ψJT 0.5 K/W)",
        efficiency=BQ25798_EFFICIENCY[750e3], sw_high=1.15,  # data sheet curve is typical; high corner +15 %
        charge_a=1.8, charge_note="ICHG register set by the ESP32-S3 (1 A default without a host)",
        losses=("BQ25798 buck", "BQ25798 BATFET"),
        emi_loop=(("C44", "1"), ("U4", "29"), ("U4", "27"), ("C44", "2")),  # 0.1 uF PMID capacitor at the pins, PMID pin, GND pin
        emi_caps=("C44",),
        emi_fsw_options=(750e3, 1.5e6),  # PROG default and a 1.5 MHz variant that needs L1 = 1 uH, both reported
        l1=dict(label="FC-ALX4030D-2R2MT", dcr=10.7e-3, l=2.2e-6),  # LCSC C5370906 data sheet: 10.7 mΩ typical, 7.3 A minimum saturation
        bulk_caps=("C1", "C6", "C9", "C10", "C11", "C13", "C14", "C18", "C19", "C20", "C24", "C25", "C26",
                   "C33", "C41", "C42", "C47"),  # X7R; the 0.1 uF and REGN parts are not bulk
        usb_cases=[
            dict(key="usb37", label="USB 3 A source, cell 3.7 V, 1.8 A host mode, TREG 100 °C, 0.8 A load", vcell=3.7, load=0.8,
                 charge=1.8, treg=100.0),
            dict(key="usb32", label="USB 3 A source, cell 3.2 V, 1.8 A host mode, TREG 100 °C, 0.8 A load", vcell=3.2, load=0.8,
                 charge=1.8, treg=100.0),
            dict(key="usb32off", label="USB 3 A source, cell 3.2 V, 1.8 A host mode, TREG 100 °C, device off", vcell=3.2,
                 load=0.0, charge=1.8, treg=100.0),
            dict(key="usb37def", label="USB 3 A source, cell 3.7 V, 1 A default mode, TREG 120 °C, 0.8 A load", vcell=3.7,
                 load=0.8, charge=1.0, treg=120.0),
            dict(key="usb32def", label="USB 3 A source, cell 3.2 V, 1 A default mode, TREG 120 °C, 0.8 A load", vcell=3.2,
                 load=0.8, charge=1.0, treg=120.0),
        ],
        # the agreed sink: the same 10 x 10 mm aluminium sink on the package top as on the MAX board. The
        # package publishes RθJC(top) 20.9 K/W (no exposed pad). A bottom-side spreader plate was a thought
        # experiment only (type "plate" in run_heat.py, not a design).
        heatsink={"U4": dict(type="top", label="10 x 10 mm aluminium sink (Fischer ICK S 10 x 10 x 6.5) on a 1.5 mm "
                                               "copper spacer, thermal epoxy", size_mm=10.0,
                             r_jt=20.9,  # K/W, RθJC(top), SLUSDV2C thermal table
                             r_tim=20.0,  # K/W, two 0.1 mm epoxy layers (k 1.2 W/m·K) on the 16 mm² top plus the spacer
                             r_sink_open=35.0)},  # K/W, sink to still air, Fischer figure derated for 30 to 40 K
        ts_hot=43.0, ts_restart=41.6,  # C, T5 acts / releases with RT1 80.6k / RT2 768k (thermal/results_ti.md)
        foldback_width=2.0,  # K: the analog loop holds the junction at TREG
        default_a=1.0,  # ICHG after power-on or watchdog expiry
        transient_policies=[("default", "1 A default mode, TREG 120 °C", 1.0, 120.0, None),
                            ("host", "1.8 A host mode, TREG 100 °C", 1.8, 100.0, None),
                            ("host80", "1.8 A host mode, TREG 80 °C", 1.8, 80.0, None)],
    ),
}


def for_project(path=None):
    tag = project.charger(project.resolve(path))
    return tag, CHARGERS[tag]


def case_table(chg):
    """Operating-point list of run_heat: battery cases plus the charger's USB cases."""
    cases = [
        {"key": "battyp", "label": "Battery 3.1 V, typical 1.22 W load", "mode": "battery", "vin": 3.1,
         "load": 1.22 / 5.06, "ambients": (25.0, 50.0)},
        {"key": "bat08", "label": "Battery 3.1 V, 0.8 A load", "mode": "battery", "vin": 3.1, "load": 0.8,
         "ambients": (25.0, 50.0)},
        {"key": "bat10", "label": "Battery 3.1 V, 1.0 A load", "mode": "battery", "vin": 3.1, "load": 1.0,
         "ambients": (25.0, 50.0)},
    ]
    for c in chg["usb_cases"]:
        cases.append(dict(c, mode="usb", limit=3.0, ambients=(25.0,), charge=c.get("charge", chg["charge_a"]),
                          treg=c.get("treg", chg["treg"])))
    return cases
