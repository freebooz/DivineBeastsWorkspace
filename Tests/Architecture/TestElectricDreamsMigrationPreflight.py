#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Electric Dreams资源准入门禁离线回归：仅造临时文本夹具，不生成UE资产。

覆盖：未下载、假的不完整工程、具备结构的候选工程、公共Git忽略规则、
路径失配与禁止覆盖正式地图的状态口径。临时夹具不代表Epic真实授权。
"""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "Tools/Unreal/PCG/InspectElectricDreamsSample.py"
NAME = "electric_dreams_source_preflight"
SPEC = importlib.util.spec_from_file_location(NAME, TOOL)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("没有找到原始官方样例准入工具")
mod = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mod)


class ElectricDreamsSourcePreflightTests(unittest.TestCase):
    """真实源码执行合同，避免将模拟源目录误认正式资源。"""

    def test_missing_source_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            r = mod.inspect_source(Path(tmp)/"not-downloaded", ROOT)
            self.assertEqual(r["sourceStatus"], "source_missing")
            self.assertFalse(r["fabEntitlementVerified"])
            self.assertFalse(r["migrationPerformed"])

    def test_partial_sample_is_not_ready(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            r = Path(tmp)/"Sample"
            (r/"Content").mkdir(parents=True)
            (r/"ElectricDreamsEnv.uproject").write_text(
                json.dumps({"FileVersion": 3, "EngineAssociation": "5.2"}), encoding="utf-8")
            result = mod.inspect_source(Path(tmp), ROOT)
            self.assertEqual(result["sourceStatus"], "source_incomplete")
            self.assertFalse(result["forestMapPresent"])

    def test_sample_structure_is_not_entitlement(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)/"ElectricDreamsEnv"
            sample_map = root/"Content"/mod.FOREST_MAP_REL
            sample_map.parent.mkdir(parents=True)
            (root/"ElectricDreamsEnv.uproject").write_text(
                json.dumps({"FileVersion": 3, "EngineAssociation": "5.2"}), encoding="utf-8")
            # 只有森林目录与普通README，不伪造任何.umap或.uasset文件。
            (sample_map.parent/"README.txt").write_text("This is only a test directory", encoding="utf-8")
            result = mod.inspect_source(Path(tmp), ROOT)
            self.assertEqual(result["sourceStatus"], "source_incomplete")
            self.assertEqual(result["assetCount"], 0)
            self.assertFalse(result["forestMapPresent"])
            self.assertFalse(result["fabEntitlementVerified"])
            self.assertFalse(result["assetRegistryVerified"])
            self.assertFalse(result["migrationPerformed"])
            self.assertFalse(result["villageMapModified"])

    def test_public_git_fences(self) -> None:
        r = mod.inspect_git_protection(ROOT)
        self.assertTrue(r["gitCheckAvailable"])
        self.assertTrue(r["safeForPublicGit"])
        self.assertFalse(r["trackedThirdPartyPaths"])

    def test_current_contract_not_imported(self) -> None:
        data = mod.inspect(ROOT, Path("D:/UnrealSamples"))
        self.assertEqual(data["projectCode"], "SSLM")
        self.assertFalse(data["mayMigrateWithoutFabEntitlementCheck"])
        self.assertEqual(data["targetMountPath"], "/DBAWorldPack_Village/Environment/ThirdParty/ElectricDreams")


if __name__ == "__main__":
    unittest.main()
