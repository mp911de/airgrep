"""Electrical regressions for the module feed model, including reverse isolation."""
import math
import unittest

import run_power as power
import simlib


@unittest.skipUnless(simlib.NGSPICE, "ngspice is required")
class ModuleFeedTests(unittest.TestCase):
    def bench(self, supplies, loads):
        bench = simlib.Bench(power.BENCHES)
        feeds = simlib.flatten(power.BENCHES + "/module_feeds.inc")
        bench.decks["feeds"] = ("Module feed regression\n"
                                ".param cmod=40u\n.param rmod=20m\n" + feeds + "\n" + supplies + "\n" + loads + "\n"
                                ".tran 1u 3m\n.control\nrun\n"
                                "wrdata feeds.txt v(rail_5v) v(ads_5v_d) v(esp_5v_d) "
                                "v(ads_5v) v(esp_5v) i(Vrail) i(VU11) i(VU12)\n.endc\n.end\n")
        return bench

    def test_forward_drop_and_branch_currents(self):
        bench = self.bench("Vrail RAIL_5V 0 5.06", "Vload LOADF 0 PWL(0 .01 1m .01 1.01m .8 2m .8 2.01m 1.5)")
        for name, corner in power.ID_CORNERS.items():
            with self.subTest(corner=name):
                rows = bench.simulate("feeds", ads_share=power.ADS_SHARE, **corner)
                for time, load in ((.0009, .01), (.0019, .8), (.0029, 1.5)):
                    row = next(row for row in rows if row[0] >= time)
                    self.assertAlmostEqual(-row[6], load, delta=1e-6)
                    for share, voltage, terminal, current in ((power.ADS_SHARE, row[2], row[4], row[7]),
                                                              (1-power.ADS_SHARE, row[3], row[5], row[8])):
                        self.assertAlmostEqual(current, load * share, delta=1e-6)
                        expected = max(corner["id_vfwd"], corner["id_ron"] * current)
                        # Native blocking diode adds less than 0.6 mV, documented in the model.
                        self.assertAlmostEqual(row[1] - voltage, expected, delta=.0006)
                        self.assertAlmostEqual(voltage - terminal, current * .020, delta=1e-7)

    def test_usb_power_cannot_backfeed_rail_or_other_module(self):
        for module, other_col in (("ADS_5V", 5), ("ESP_5V", 4)):
            for rail in (0, 5.06):
                with self.subTest(module=module, rail=rail):
                    bench = self.bench("Vrail RAIL_5V 0 %s\nVusb %s 0 5.2" % (rail, module),
                                       "Vload LOADF 0 .1")
                    row = bench.simulate("feeds", ads_share=power.ADS_SHARE, **power.ID_CORNERS["maximum"])[-1]
                    self.assertLessEqual(row[6], 1e-6, "rail must not sink backfeed current")
                    blocked_current = row[7 if module == "ADS_5V" else 8]
                    self.assertLess(abs(blocked_current), 1e-6)
                    self.assertLess(row[other_col], rail + 1e-3)

    def test_shutdown_completes_without_solver_failure(self):
        for name, corner in power.ID_CORNERS.items():
            with self.subTest(corner=name):
                params = dict(power.transient_params(3.7, .8, t1=1., toff=.03), **corner)
                rows = power.BENCH.simulate("boost_transient", **params)
                self.assertAlmostEqual(rows[-1][0], .045)
                self.assertTrue(all(math.isfinite(v) for row in rows for v in row))
                for col in (10, 11):
                    self.assertLess(rows[-1][col], .5)
                    self.assertGreater(min(row[col] for row in rows), -.05)
