"""Energy balance, thermal inertia and charge-control checks for the reduced model."""
import math
import unittest

import numpy as np
from scipy.optimize import brentq

import run_charge_heat as charge
import run_heat as heat
import run_charge_limits as limits
import vent_model

FULL = ("1.8 A test", 1.8, 130.0, None)  # charger-independent policy for the switch and limit tests
EARLY = ("early test", 1.8, 130.0, (90.0, 100.0))


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
        hot, restart = charge.HOT, charge.RESTART
        mid = (hot + restart) / 2
        self.assertTrue(charge.jeita_hot(False, hot))
        self.assertTrue(charge.jeita_hot(True, mid))
        self.assertFalse(charge.jeita_hot(True, restart))
        self.assertFalse(charge.jeita_hot(False, mid))
        self.assertEqual(charge.current_target(80, True, FULL), 0)

    def test_step_size_and_energy_residual(self):
        runs = [charge.simulate(calibration(), charge.PACKS["5000"], policy=EARLY, dt=dt)
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


class TemperatureSwitchTests(unittest.TestCase):
    def test_hysteresis_remembers_trip_until_lower_threshold(self):
        for width in (5, 10):
            switch = charge.TemperatureSwitch(90, width)
            state = False
            for temperature, expected in ((89, False), (90, True), (89, True),
                                           (90 - width + .1, True), (90 - width, False), (89, False)):
                state = switch.tripped(state, temperature)
                self.assertEqual(state, expected)

    def test_no_effect_below_threshold(self):
        plain, _ = charge.simulate(calibration(), charge.PACKS["5000"], policy=FULL, minutes=3)
        switched, _ = charge.simulate(calibration(), charge.PACKS["5000"], policy=FULL, minutes=3,
                                     limit=charge.TemperatureSwitch(125))
        np.testing.assert_array_equal(plain, switched[:, :8])

    def test_stop_restart_and_pack_precedence(self):
        switch = charge.TemperatureSwitch(80, 10)
        data, error = charge.simulate(calibration(), charge.PACKS["5000"], policy=FULL, limit=switch, dt=.25)
        state = data[:, 9].astype(bool)
        rising = np.flatnonzero(np.diff(state.astype(int), prepend=0) == 1)
        falling = np.flatnonzero(np.diff(state.astype(int), prepend=0) == -1)
        self.assertGreater(len(rising), 1)
        self.assertGreater(len(falling), 1)
        self.assertTrue(np.all(data[rising, 8] >= 80))
        self.assertTrue(np.all(data[falling, 8] <= 70))
        self.assertTrue(np.all(data[state, 6] == 0))
        self.assertTrue(np.any(data[:, 6] > 1.79))
        pack_hot = data[:, 5] >= charge.HOT
        self.assertTrue(np.any(pack_hot))
        self.assertTrue(np.all(data[pack_hot, 6] == 0))
        self.assertLess(error, 1e-8)

    def test_wider_hysteresis_reduces_cycle_frequency(self):
        stats = []
        for width in (5, 10):
            data, error = charge.simulate(calibration(), charge.PACKS["10000"], policy=FULL,
                                         limit=charge.TemperatureSwitch(85, width), dt=.25)
            stats.append(limits.summary(data, error))
        self.assertLess(stats[1]["charger_stops"], stats[0]["charger_stops"])
        self.assertGreater(stats[1]["cycle_median_s"], stats[0]["cycle_median_s"])

    def test_local_sensor_is_not_a_junction_ceiling(self):
        peaks = []
        for node, tau in ((0, 0), (1, 2)):
            data, _ = charge.simulate(calibration(), charge.PACKS["10000"], policy=FULL, minutes=20, dt=.25,
                                     limit=charge.TemperatureSwitch(90, sensor_node=node, sensor_tau=tau))
            peaks.append(max(data[:, 1]))
        self.assertLess(peaks[0], 91)
        self.assertGreater(peaks[1], peaks[0] + 3)

    def test_switching_converges_with_smaller_timestep(self):
        results = []
        for dt in (.25, .125):
            data, error = charge.simulate(calibration(), charge.PACKS["10000"], policy=FULL, dt=dt, minutes=35,
                                         limit=charge.TemperatureSwitch(90))
            results.append(limits.summary(data, error))
        a, b = results
        self.assertLess(abs(a["charge_ah"] - b["charge_ah"]), .003)
        self.assertLess(abs(a["peak_u4"] - b["peak_u4"]), .2)
        self.assertLess(abs(a["cycle_median_s"] - b["cycle_median_s"]), 1)

    def test_current_average_preserves_charge(self):
        data = np.zeros((481, 8))
        data[:, 0] = np.arange(481) * .25 / 60
        data[:120, 6] = 1.8
        data[240:480, 6] = .6
        _, means = limits.average_current(data)
        np.testing.assert_allclose(means, [.9, .6])
        self.assertAlmostEqual(sum(means) / 60, np.sum(data[:-1, 6]) * .25 / 3600)


if __name__ == "__main__":
    unittest.main()
