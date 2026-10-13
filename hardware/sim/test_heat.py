"""Analytic regressions for the board's electrical and thermal solvers."""
import json
import math
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import numpy as np

import run_heat as heat
import run_power
import vent_model


def sheet():
    network = heat.Sheet(heat.Grid((0, 0, 2, 2)))
    for layer in range(heat.NL):
        network.set_sheet(layer, [1.0] * network.grid.nc)
        network.gref[layer] = [1.0] * network.grid.nc
    return network


def track_board(layer="B", length=4.0):
    pads = [SimpleNamespace(ref=ref, num="1", x=x, y=0.5, w=0.2, h=0.2,
                            layers=(layer,), net="TEST", tht=False)
            for ref, x in (("A", 0.5), ("B", 0.5 + length))]
    return {"pads": pads, "segments": [(0.5, 0.5, 0.5 + length, 0.5, 1.0, layer, "TEST")],
            "vias": [], "zones": [], "polys": []}


class SheetTests(unittest.TestCase):
    def test_zero_source(self):
        network = sheet()
        self.assertEqual(network.solve([0.] * 16), [0.] * 16)

    def test_uniform_source(self):
        network = sheet()
        for x in network.solve([2.] * 16):
            self.assertAlmostEqual(x, 2.0)

    def test_singular_system_is_rejected(self):
        network = heat.Sheet(heat.Grid((0, 0, 1, 1)))
        with self.assertRaisesRegex(RuntimeError, "singular"):
            network.solve([1.] * 4)

    def test_edges_layers_and_junction_conserve_flow(self):
        network = sheet()
        for layer in range(heat.NL - 1):
            network.gz[layer][0] = 2.0
        network.add_node(0.5, [(0, 3, 3.0), (2, 0, 4.0)])
        expected = [float(i + 1) for i in range(17)]
        source = expected[:16] + [0.5 * expected[16]]
        edges = [(4 * layer + a, 4 * layer + b, 1.0) for layer in range(heat.NL)
                 for a, b in ((0, 1), (2, 3), (0, 2), (1, 3))]
        edges += [(4 * layer, 4 * layer + 4, 2.0) for layer in range(heat.NL - 1)]
        edges += [(16, 3, 3.0), (16, 8, 4.0)]
        for a, b, conductance in edges:
            flow = conductance * (expected[a] - expected[b])
            source[a] += flow
            source[b] -= flow
        for actual, wanted in zip(network.solve(source), expected):
            self.assertAlmostEqual(actual, wanted)

    def test_factorization_owns_a_snapshot(self):
        network = sheet()
        original = network.factorize()
        network.gref[0] = [2.] * 4
        for x in original.solve([2.] * 16):
            self.assertAlmostEqual(x, 2.)
        for x in network.solve([2.] * 16)[:4]:
            self.assertAlmostEqual(x, 1.)

    def test_nonfinite_source_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "finite"):
            sheet().solve([float("nan")] * 16)

    def test_unmet_residual_tolerance_is_rejected(self):
        system = sheet().factorize()
        with self.assertRaisesRegex(RuntimeError, "residual"):
            with patch.object(system, "_solve", return_value=np.zeros(16)):
                system.solve([1.] * 16)


class TrackTests(unittest.TestCase):
    def setUp(self):
        self.grid = heat.Grid((0, 0, 6, 2))
        self.net = heat.TrackNet(track_board(), self.grid, "TEST")
        self.src, self.dst = ("A", ("1",)), ("B", ("1",))

    def test_resistance_and_power(self):
        resistance = heat.RHO_CU * 0.004 / (0.001 * heat.CU_T["B"])
        volts, power, loss_map = self.net.solve({self.src: 2.0, self.dst: -2.0})
        self.assertAlmostEqual(volts[("pad", "A", "1")] - volts[("pad", "B", "1")], 2 * resistance)
        self.assertAlmostEqual(power, 4 * resistance)
        self.assertAlmostEqual(sum(loss_map.values()), power)

    def test_zero_current(self):
        volts, power, loss_map = self.net.solve({self.src: 0., self.dst: 0.})
        self.assertTrue(all(v == 0 for v in volts.values()))
        self.assertEqual(power, 0)
        self.assertEqual(sum(loss_map.values()), 0)

    def test_unbalanced_current_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "balance"):
            self.net.solve({self.src: 1., self.dst: -0.5})

    def test_equipotential_terminals_need_no_matrix_solve(self):
        net = heat.TrackNet(track_board(length=0.), self.grid, "TEST")
        volts, power, _ = net.solve({self.src: 1., self.dst: -1.})
        self.assertEqual(volts[("pad", "A", "1")], volts[("pad", "B", "1")])
        self.assertEqual(power, 0.)

    def test_temperature_belongs_to_copper_layer(self):
        nc = self.grid.nc
        cold = [20.] * (heat.NL * nc)
        resistance = self.net.resistance(self.src, self.dst, cold)
        for layer in range(heat.NL):
            hot = cold[:]
            hot[layer * nc:(layer + 1) * nc] = [120.] * nc
            expected = resistance * (1 + 100 * heat.ALPHA_CU) if layer == 3 else resistance
            with self.subTest(layer=layer):
                self.assertAlmostEqual(self.net.resistance(self.src, self.dst, hot), expected)

    def test_wide_track_end_connects_both_pad_and_via(self):
        board = track_board(layer="F", length=1.)
        board["pads"][0].w = 1.2
        board["pads"][1].layers = ("B",)
        board["vias"] = [(1.5, 0.5, 0.6, 0.3, "TEST")]
        net = heat.TrackNet(board, self.grid, "TEST")
        resistance = net.resistance(self.src, self.dst)
        self.assertTrue(math.isfinite(resistance))
        self.assertGreater(resistance, 0.)

    def test_via_uses_adjacent_layers_for_temperature_and_heat(self):
        board = track_board()
        board["segments"] = []
        for pad, layer in zip(board["pads"], ("F", "B")):
            pad.x = 0.5
            pad.layers = (layer,)
        board["vias"] = [(0.5, 0.5, 0.6, 0.3, "TEST")]
        net = heat.TrackNet(board, self.grid, "TEST")
        temperatures = (20., 40., 100., 120.)
        temps = [t for t in temperatures for _ in range(self.grid.nc)]
        expected_heat = [0.] * heat.NL
        for k, span in enumerate(heat.VIA_SPAN):
            resistance = heat.RHO_CU * span / (math.pi * 0.3e-3 * heat.T_PLATING)
            resistance *= 1 + heat.ALPHA_CU * ((temperatures[k] + temperatures[k + 1]) / 2 - 20)
            expected_heat[k] += 4 * resistance / 2
            expected_heat[k + 1] += 4 * resistance / 2
        _, power, loss_map = net.solve({self.src: 2., self.dst: -2.}, temps)
        self.assertAlmostEqual(power, sum(expected_heat))
        for layer, expected in enumerate(expected_heat):
            self.assertAlmostEqual(sum(p for (li, _), p in loss_map.items() if li == layer), expected)


class OperatingPointTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        board = heat.read_board()
        with open(heat.PARAMS) as source:
            params = json.load(source)
        k_sw, p0, _ = heat.fit_boost(params["boost_efficiency_curve"])
        cls.k_sw = k_sw
        copper = heat.Copper(board)
        cls.model = heat.Model(board, copper, (k_sw, p0), params, 0)
        cls.high = heat.Model(board, copper, (k_sw, p0), params, 1, heat.HEATSINK)
        paths = heat.solve_paths(cls.model, board)
        for model in (cls.model, cls.high):
            model.r_net, model.r_gnd_bat, model.r_gnd_usb = paths
        cls.case = next(c for c in heat.CASES if c["key"] == "usb37")

    def test_ground_losses_follow_the_operating_point(self):
        with patch.object(self.model, "gnd_solve", wraps=self.model.gnd_solve) as ground:
            result = heat.run_case(self.model, self.case, "closed", 25., self.k_sw)
        self.assertGreater(result["iterations"], 1)
        self.assertEqual(ground.call_count, result["iterations"])
        self.assertIsNotNone(ground.call_args.args[1])

    def test_case_does_not_inherit_voltage_drop(self):
        first = heat.run_case(self.model, self.case, "closed", 25., self.k_sw)
        other = next(c for c in heat.CASES if c["key"] == "usb32")
        heat.run_case(self.model, other, "open", 25., self.k_sw)
        second = heat.run_case(self.model, self.case, "closed", 25., self.k_sw)
        self.assertEqual(first["tj"], second["tj"])

    def test_unconverged_case_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, "thermal iteration did not converge"):
            heat.run_case(self.model, self.case, "closed", 25., self.k_sw, max_iterations=1)

    def test_vents_reduce_pack_temperature(self):
        case = next(c for c in heat.CASES if c["key"] == "bat08")
        vented = heat.run_case(self.high, case, "closed", 25., self.k_sw)
        sealed = heat.run_case(self.high, case, "closed", 25., self.k_sw, vent_area_cm2=0.)
        self.assertLess(vented["t_pack"], sealed["t_pack"])

    def test_battery_path_copper_in_the_power_model_follows_the_board(self):
        """run_power.R_BOARD copies J3 to F1, F1 to U4 BATT and U4 SYS to L2 from the heat model."""
        board = sum(self.model.r_net[n] for n in ("BAT_IN", "VBAT", "VSYS"))
        self.assertAlmostEqual(run_power.R_BOARD, board, delta=0.15e-3)

    def test_vent_model_constants_follow_the_heat_model(self):
        """The sealed vent model must reproduce the sealed case it copies its constants from."""
        for key, scenario in (("bat08", "bat08"), ("usb32", "usb32"), ("usb32off", "usb_off")):
            case = next(c for c in heat.CASES if c["key"] == key)
            grid = heat.run_case(self.high, case, "closed", 25., heat.buck_k(self.k_sw) * heat.BUCK_SW_HIGH, vent_area_cm2=0.)
            lumped = vent_model.solve(scenario, 25., area_cm2=0.)
            with self.subTest(case=key):
                self.assertAlmostEqual(lumped["t_air"], grid["t_air"], delta=1.0)
                self.assertAlmostEqual(lumped["t_pack"], grid["t_pack"], delta=1.0)
                self.assertAlmostEqual(lumped["i_chg"], grid["i_chg"], delta=0.05)


class EnclosureTests(unittest.TestCase):
    def test_sealed_air_rise_matches_wall_resistance(self):
        self.assertAlmostEqual(vent_model.air_rise(5., 25., area_cm2=0.), 5. * vent_model.R_WALL)

    def test_vents_conserve_heat_and_lower_temperature(self):
        rise = vent_model.air_rise(5., 25.)
        cooling = rise / vent_model.R_WALL + vent_model.vent_power(
            rise, vent_model.VENT_AREA_CM2, vent_model.VENT_HEIGHT_M, 25.)
        self.assertAlmostEqual(cooling, 5.)
        self.assertGreater(rise, 0.)
        self.assertLess(rise, vent_model.air_rise(5., 25., area_cm2=0.))
        self.assertLess(vent_model.air_rise(5., 25., area_cm2=2 * vent_model.VENT_AREA_CM2), rise)


if __name__ == "__main__":
    unittest.main()
