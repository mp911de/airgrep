"""Closed-form and board-reading regressions for the EMI estimate. No ngspice needed."""
import math
import os
import tempfile
import unittest

import numpy as np

import run_emi as emi
import run_power
import simlib


class SpectrumTests(unittest.TestCase):
    def test_period_lines_match_rectangular_pulse(self):
        # one rectangular pulse of 5 V, 40 % duty at 1 MHz: 2 V D |sinc(n D)|
        fsw, duty, v = 1e6, 0.4, 5.0
        t = np.arange(0, 1 / fsw, 10e-12)
        x = np.where((t > 100e-9) & (t <= 100e-9 + duty / fsw), v, 0.0)
        freqs = fsw * np.arange(1, 11)
        lines = emi.period_lines(t, x, 10e-12, fsw, freqs)
        closed = emi.trapezoid_lines(v, duty, 0.0, fsw, freqs)
        for n, (a, b) in enumerate(zip(lines, closed), 1):
            if b > 0.05:
                self.assertAlmostEqual(a / b, 1.0, delta=0.03, msg="harmonic %d" % n)

    def test_trapezoid_edge_rolloff(self):
        # a 5 ns edge is 40 dB per decade down beyond 1/(pi tr), the plateau is flat below;
        # odd harmonics of a 50 % square wave, the even ones are nulls
        lines = emi.trapezoid_lines(1.0, 0.5, 5e-9, 1e6, np.array([1e6, 3e6, 637e6, 6.367e9]))
        self.assertAlmostEqual(lines[0], 2 / math.pi, delta=0.01)
        self.assertGreater(lines[2] / lines[3], 50)

    def test_single_edge_spectrum_is_v_over_omega_at_low_frequency(self):
        spec = emi.single_edge_spectrum(3.3, 2e-9, np.array([1e6]))
        self.assertAlmostEqual(spec[0], 3.3 / (2 * math.pi * 1e6), delta=1e-9)

    def test_windowed_lines_recover_a_sine(self):
        t = np.arange(0, 40e-6, 0.5e-9)
        x = 0.01 * np.sin(2 * math.pi * 721e3 * t) + 0.002 * np.sin(2 * math.pi * 3 * 721e3 * t)
        lines = emi.windowed_lines(t, x, 1e-9, np.array([721e3, 2 * 721e3, 3 * 721e3]))
        self.assertAlmostEqual(lines[0], 0.01, delta=0.0005)
        self.assertLess(lines[1], 0.0005)
        self.assertAlmostEqual(lines[2], 0.002, delta=0.0002)


class FieldTests(unittest.TestCase):
    def test_loop_field_against_ott(self):
        # Ott: 1 cm² loop, 1 mA at 100 MHz, 3 m, with ground reflection: 2.63e-14 f² A I / r
        e = emi.loop_field(100e6, 1e-4, 1e-3, r=3.0, ground=True)
        expected = 2.63e-14 * 1e16 * 1e-4 * 1e-3 / 3  # kr = 6.3: the near-field term adds 1.3 %
        self.assertAlmostEqual(e / expected, 1.0, delta=0.02)
        self.assertAlmostEqual(emi.loop_field(100e6, 1e-4, 1e-3, ground=False) / expected, 0.5, delta=0.01)

    def test_near_field_term(self):
        far = emi.loop_field(1e9, 1e-6, 1e-3, r=10.0, ground=False) * 10
        near = emi.loop_field(1e9, 1e-6, 1e-3, r=0.0477, ground=False) * 0.0477  # kr = 1
        self.assertAlmostEqual(near / far, math.sqrt(2), delta=0.01)

    def test_received_power_of_a_dipole(self):
        # 1 V/m at 1 GHz into an isotropic antenna: E²/η · λ²/(4π) = 19 µW
        p = emi.received_power(1.0, 1e9, gain=1.0)
        self.assertAlmostEqual(p * 1e6, 19.0, delta=0.5)

    def test_limits(self):
        self.assertEqual(emi.radiated_limit(100e6), 40.0)
        self.assertEqual(emi.radiated_limit(500e6), 47.0)
        self.assertIsNone(emi.radiated_limit(10e6))
        self.assertAlmostEqual(emi.conducted_limit(150e3), 66.0)
        self.assertAlmostEqual(emi.conducted_limit(500e3), 56.0)
        self.assertAlmostEqual(emi.conducted_limit(10e6), 60.0)
        self.assertAlmostEqual(emi.conducted_limit(math.sqrt(150e3 * 500e3)), 61.0)
        self.assertEqual(emi.rbw_at(10e6), 9e3)
        self.assertEqual(emi.rbw_at(2e9), 1e6)

    def test_merged_lines_add_as_power(self):
        self.assertAlmostEqual(emi.merged(1.0, 100e3, 400e3), 2.0)
        self.assertEqual(emi.merged(1.0, 1e6, 120e3), 1.0)


class FigureTests(unittest.TestCase):
    def test_pdf_cross_reference_points_at_objects(self):
        fig = simlib.Figure(100, 50)
        fig.line([(0, 0), (10, 10)], "#ff0000", dash=(2, 1))
        fig.rect(1, 1, 5, 5, fill="#00ff00", stroke="#000000")
        fig.circle(20, 20, 3, "#0000ff")
        fig.text(30, 30, "(−121 dBm) 250 kHz µ", rotate=True, anchor="middle")
        with tempfile.TemporaryDirectory() as d:
            fig.pdf(os.path.join(d, "f.pdf"))
            data = open(os.path.join(d, "f.pdf"), "rb").read()
        self.assertTrue(data.startswith(b"%PDF-1.4") and data.endswith(b"%%EOF\n"))
        xref = int(data.rsplit(b"startxref\n", 1)[1].split()[0])
        offsets = [int(row[:10]) for row in data[xref:].split(b"\n")[3:9]]
        for n, at in enumerate(offsets, 1):
            self.assertTrue(data[at:].startswith(b"%d 0 obj" % n), n)


class GeometryTests(unittest.TestCase):
    def test_microstrip(self):
        z0, lp, cp = emi.microstrip(0.35e-3)
        self.assertTrue(45 < z0 < 65, z0)
        self.assertAlmostEqual(lp * cp, 1 / emi.C0 ** 2 * 3.0, delta=0.3 / emi.C0 ** 2)
        wide, _, _ = emi.microstrip(2e-3)
        self.assertLess(wide, z0)

    def test_polygon_and_point(self):
        square = [(0, 0), (2, 0), (2, 2), (0, 2)]
        self.assertEqual(emi.polygon_area(square), 4.0)
        self.assertTrue(emi.inside((1, 1), square))
        self.assertFalse(emi.inside((3, 1), square))
        self.assertTrue(emi.segments_cross((0, 0, 2, 2, 0.2, "B", "A"), (0, 2, 2, 0, 0.2, "In2", "B")))
        self.assertFalse(emi.segments_cross((0, 0, 1, 0, 0.2, "B", "A"), (0, 1, 1, 1, 0.2, "In2", "B")))


class BoardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.board = emi.Board()

    def test_hot_loops_are_small_and_closed(self):
        for name, conv in emi.CONVERTERS.items():
            loop = self.board.hot_loop(conv["loop"], conv["caps"])
            self.assertTrue(1e-6 < loop["area"] < 20e-6, (name, loop["area"]))
            self.assertTrue(1e-9 < loop["l_total"] < 10e-9, (name, loop["l_total"]))
            self.assertEqual(len(loop["runs"]), 2, name)
            nets = {self.board.pad(*p).net for p in conv["loop"]}
            self.assertIn("GND", nets)
            self.assertIn(conv["loop"][0][0] if conv["dir"] < 0 else conv["loop"][1][0], ("C18",) + tuple(run_power.CHG["emi_caps"]))

    def test_in1_is_solid_ground(self):
        self.assertEqual({n for n, lay, _ in self.board.data["zones"] if lay == "In1"}, {"GND"})

    def test_digital_nets_exist(self):
        for net, *_ in emi.DIGITAL:
            self.assertTrue(self.board.net_tracks(net), net)

    def test_mounts_and_inductors(self):
        for _, _, _, _, mount in emi.RECEIVERS:
            self.assertIn(mount, self.board.mounts)
        for conv in emi.CONVERTERS.values():
            self.board.position(conv["inductor"])

    def test_return_path_gaps_are_short(self):
        gaps = self.board.return_path_gaps("B", "In2")
        digital = [d[0] for d in emi.DIGITAL]
        self.assertLess(sum(v[0] + v[2] for n, v in gaps.items() if n in digital), 5.0)


if __name__ == "__main__":
    unittest.main()
