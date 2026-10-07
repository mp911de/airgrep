"""Energy balance, thermal inertia and charge-control checks for the reduced model."""
import math
import unittest

import numpy as np
from scipy.optimize import brentq

import run_charge_heat as charge
import run_heat as heat
import vent_model


def calibration():
    return charge.Calibration(np.array([0.0, 1.8]), np.array([[.3, .1, 4.5], [1.8, .2, 5.0]]),
                              45.0, 55.0, 58.0, .09, [])


class TransientTests(unittest.TestCase):
    def test_single_thermal_mass_matches_exponential(self):
        c, r, power, dt = 80.0, 10.0, 2.0, .5
        net = charge.Network(np.array([c]), np.array([[1 / r]]), np.array([1 / r]))
        step = net.stepper(dt)
        temperature = np.array([25.0])
        for _ in range(round(c * r / dt)):
            temperature = step(temperature, np.array([power]), 25.0)
        expected = 25 + power * r * (1 - math.exp(-1))
        self.assertAlmostEqual(temperature[0], expected, delta=.003)

    def test_hot_charger_does_not_instantly_heat_pack(self):
        net = charge.network(calibration(), charge.PACKS["5000"])
        temperatures = net.stepper(1)(np.full(5, 25.0), np.array([2., 0., 0., 0., 0.]), 25)
        self.assertGreater(temperatures[0] - 25, 10)
        self.assertLess(temperatures[4] - 25, .001)

    def test_shared_air_steady_state_and_energy_conservation(self):
        cal, pack = calibration(), charge.PACKS["10000"]
        net = charge.network(cal, pack)
        current, ambient = 1.2, 25.0
        pu, pb, other = cal.losses(current)
        power = np.array([pu, pb, 0.0, other, current ** 2 * cal.r_pack_dc])
        rise = vent_model.air_rise(sum(power), ambient, wall_resistance=heat.enclosure_resistance()[0])
        power[3] -= vent_model.vent_power(rise, vent_model.VENT_AREA_CM2,
                                          vent_model.VENT_HEIGHT_M, ambient)
        temperature = np.linalg.solve(net.g, power + net.ambient_g * ambient)
        self.assertAlmostEqual(temperature[3], ambient + rise, places=9)
        a, b, d = np.array(pack["mm"]) / 1000
        r_pack = 1 / (heat.H_IN * 2 * (a * b + a * d + b * d))
        self.assertAlmostEqual(temperature[4] - temperature[3], current ** 2 * cal.r_pack_dc * r_pack)
        self.assertAlmostEqual(sum(power), np.dot(net.ambient_g, temperature - ambient), places=9)

    def test_jeita_hysteresis(self):
        self.assertTrue(charge.jeita_hot(False, 42.1))
        self.assertTrue(charge.jeita_hot(True, 41.5))
        self.assertFalse(charge.jeita_hot(True, 40.8))
        self.assertFalse(charge.jeita_hot(False, 41.5))
        self.assertEqual(charge.current_target(80, True, "stock"), 0)

    def test_step_size_and_energy_residual(self):
        runs = [charge.simulate(calibration(), charge.PACKS["5000"], policy="early", dt=dt)
                for dt in (1.0, .5)]
        for _, error in runs:
            self.assertLess(error, 1e-8)
        a, b = (run[0] for run in runs)
        self.assertLess(abs(a[-1, 7] - b[-1, 7]), .005)
        self.assertLess(abs(max(a[:, 1]) - max(b[:, 1])), .1)

    def test_external_path_removes_heat_without_heating_pack(self):
        cal, pack = calibration(), charge.PACKS["10000"]
        def steady(resistance):
            net = charge.network(cal, pack, spreader_r=resistance)
            pu, pb, other = cal.losses(1.8)
            p = np.array([pu, pb, 0., other, 1.8 ** 2 * cal.r_pack_dc])
            def solve(air_rise):
                source = p.copy()
                source[3] -= vent_model.vent_power(air_rise, 5, .035, 25)
                return np.linalg.solve(net.g, source + net.ambient_g * 25)
            rise = brentq(lambda delta: solve(delta)[3] - 25 - delta, 0, 100)
            return solve(rise)
        plain, external = steady(math.inf), steady(10)
        self.assertLess(external[0], plain[0])
        self.assertLess(external[4], plain[4])


if __name__ == "__main__":
    unittest.main()
