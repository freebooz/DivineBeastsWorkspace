#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Electric Dreams UE原生森林源AssetRegistry审计：纯函数回归。

仅验证归属过滤及禁止把虚构路径当作森林图，不假装已访问Fab或真实UE。
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FILE = ROOT / "Tools/Unreal/PCG/AuditElectricDreamsForestSourceUE58.py"
spec = importlib.util.spec_from_file_location("electric_dreams_source_asset_audit", FILE)
if spec is None or spec.loader is None:
    raise RuntimeError("找不到待审源资产脚本")
subject = importlib.util.module_from_spec(spec)
spec.loader.exec_module(subject)


class ElectricDreamsRegistryAuditTests(unittest.TestCase):
    def test_unreal_package_scopes(self):
        self.assertEqual(subject.categorize_package("/Game/PCG/Graphs/Forest/A"), "SampleContent")
        self.assertEqual(subject.categorize_package("/Engine/BasicShapes/Cube"), "EngineDependency")
        self.assertEqual(subject.categorize_package("/Script/PCG"), "NativeModuleDependency")
        self.assertEqual(subject.categorize_package("/Soundscape/Effects/A"), "ExternalPluginDependency")
        self.assertEqual(subject.categorize_package("Game/NoLeadingSlash"), "InvalidPackageName")

    def test_only_existing_forest_graph_candidates(self):
        paths = ["/Game/PCG/Graphs/Forest/PCGDemo_Forest", "/Game/PCG/Graphs/Meadow",
                 "/Engine/PCG/GraphForest", "/Game/ForestWithoutGraphRoot"]
        self.assertEqual(subject.select_graph_seeds(paths),
                         ["/Game/PCG/Graphs/Forest/PCGDemo_Forest"])

    def test_source_project_must_not_be_sslm(self):
        with tempfile.TemporaryDirectory() as d:
            source = Path(d) / "ElectricDreamsEnv.uproject"
            source.write_text("{}", encoding="utf-8")
            subject.validate_project_path(source)
            bad = Path(d) / "DivineBeastsArena.uproject"
            bad.write_text("{}", encoding="utf-8")
            with self.assertRaises(RuntimeError):
                subject.validate_project_path(bad)
            with self.assertRaises(RuntimeError):
                subject.validate_project_path(source, Path(d))

    def test_not_imported_by_default(self):
        self.assertIn("migrationExecuted", FILE.read_text(encoding="utf-8"))
        self.assertIn("fabOwnershipChecked", FILE.read_text(encoding="utf-8"))
        self.assertIn("sourceAssetModified", FILE.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
