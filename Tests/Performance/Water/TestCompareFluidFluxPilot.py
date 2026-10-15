# -*- coding: utf-8 -*-
"""对比工具自测：用人工合成值验证计算/门禁，不能将合成样本当作SSLM性能证据。"""

import csv
import tempfile
import unittest
from pathlib import Path

from CompareFluidFluxPilot import compare


class FluidFluxComparatorTests(unittest.TestCase):
    def make_capture(self, directory: Path, native: float, flux: float,
                     frames: int = 155) -> Path:
        path = directory / "synthetic-fixture.csv"
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.writer(stream)
            writer.writerow(["Scene", "Mode", "Quality", "Resolution", "Frame",
                             "GPUms", "GameThreadMs", "RenderThreadMs", "VRAMMiB"])
            for mode, gpu in (("Native", native), ("FluidFlux", flux)):
                for frame in range(frames):
                    writer.writerow(["CharacterStudio", mode, "High", "1920x1080",
                                     frame, gpu, 6., 5., 500.])
        return path

    def test_within_increment_budget_passes(self):
        with tempfile.TemporaryDirectory() as td:
            report = compare(self.make_capture(Path(td), 7., 8.5), 30, 120)
            self.assertTrue(report["Passed"])
            self.assertEqual(report["Results"][0]["Metrics"]["GPUms"]["AddedP95"], 1.5)

    def test_over_increment_budget_fails(self):
        with tempfile.TemporaryDirectory() as td:
            report = compare(self.make_capture(Path(td), 7., 10.1), 30, 120)
            self.assertFalse(report["Passed"])
            self.assertEqual(len(report["Failures"]), 1)

    def test_insufficient_frames_fail_closed(self):
        with tempfile.TemporaryDirectory() as td:
            report = compare(self.make_capture(Path(td), 7., 8., 80), 30, 120)
            self.assertFalse(report["Passed"])
            self.assertEqual(report["Results"], [])

    def test_invalid_gpu_rejected(self):
        with tempfile.TemporaryDirectory() as td:
            path = self.make_capture(Path(td), 7., 0.)
            with self.assertRaises(ValueError):
                compare(path, 30, 120)


if __name__ == "__main__":
    unittest.main()
