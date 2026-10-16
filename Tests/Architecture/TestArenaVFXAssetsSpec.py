#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM竞技场Niagara资源候选预算、NDC数据合同离线回归。

测试对象为项目自有JSON规格，不替代真实UE资产和GPU性能测试。
执行：python -m unittest discover -s Tests/Architecture -p TestArenaVFXAssetsSpec.py
"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
AUDIT_PATH = ROOT / "Tools/Unreal/VFX/Arena/AuditArenaVFXAssets.py"
MODULE_SPEC = importlib.util.spec_from_file_location("arena_vfx_asset_audit", AUDIT_PATH)
assert MODULE_SPEC and MODULE_SPEC.loader
MODULE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(MODULE)
ASSET_SPEC = json.loads((AUDIT_PATH.parent / "ArenaVFXAssetsSpec_V1.json").read_text(encoding="utf-8"))


class ArenaVFXAssetsSpecTests(unittest.TestCase):
    def test_budget_covers_all_quality_tiers(self):
        """七类预算各档相加准确，但只代表候选设计数值。"""
        report = MODULE.audit(ASSET_SPEC, ROOT)
        self.assertTrue(report["success"], report["errors"])
        self.assertEqual(report["candidate_system_caps"], {"Epic": 120, "High": 80, "Medium": 58, "Low": 40})
        self.assertFalse(report["unreal_native_assets_verified"])

    def test_reject_unsupported_ndc_payload_schema(self):
        """错误名称或字段类型在制作前拒绝，避免引擎实际写入失败。"""
        copy_spec = copy.deepcopy(ASSET_SPEC)
        copy_spec["data_channels"][0]["variables"][1]["type"] = "Float"
        self.assertFalse(MODULE.audit(copy_spec, ROOT)["success"])

    def test_reject_budget_sum_mismatch(self):
        """错误加总在离线门禁失败，不允许把同类和总预算混淆。"""
        copy_spec = copy.deepcopy(ASSET_SPEC)
        copy_spec["targets"]["High"] = 999
        self.assertFalse(MODULE.audit(copy_spec, ROOT)["success"])

    def test_native_files_are_not_implicitly_verified(self):
        """仅检测文件名不能声称原生资产、Shader或GPU性能通过。"""
        report = MODULE.audit(ASSET_SPEC, ROOT)
        self.assertFalse(report["gpu_performance_verified"])
        for asset in report["native_files_found"] + report["native_files_missing"]:
            self.assertTrue(asset["path"].endswith(".uasset"))


if __name__ == "__main__":
    unittest.main()
