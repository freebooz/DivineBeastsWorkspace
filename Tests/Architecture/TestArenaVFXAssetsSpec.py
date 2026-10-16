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

    def test_distinguish_three_budget_dimensions(self):
        """请求跟踪、真实System与CPU/GPU时间必须独立；误填已测或遗漏外部实例应拒绝。"""
        dimensions = ASSET_SPEC["budget_dimensions"]
        self.assertTrue(dimensions["request_tracking"]["not_niagara_system_count"])
        self.assertFalse(dimensions["real_system_instances"]["measured"])
        self.assertFalse(dimensions["frame_cost_targets"]["measured"])
        self.assertEqual(dimensions["ndc_batch_writer"]["maximum_events_per_batch"], 128)

        wrong = copy.deepcopy(ASSET_SPEC)
        wrong["budget_dimensions"]["real_system_instances"]["include_external_niagara_and_ndc_readers"] = False
        self.assertFalse(MODULE.audit(wrong, ROOT)["success"])
        wrong = copy.deepcopy(ASSET_SPEC)
        wrong["budget_dimensions"]["frame_cost_targets"]["measured"] = True
        self.assertFalse(MODULE.audit(wrong, ROOT)["success"])
        wrong = copy.deepcopy(ASSET_SPEC)
        wrong["budget_dimensions"]["ndc_batch_writer"]["multi_event_batch_only_for_global"] = False
        self.assertFalse(MODULE.audit(wrong, ROOT)["success"])

    def test_ndc_bridge_guards_location_batch_and_conflicts(self):
        """静态合同检查只防回归，真实岛屿路由与GPU Reader仍需要UE测试。"""
        bridge = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/"
                  "GamePlatformVFXClient/Private/DataChannels/GamePlatformVFXDataChannelBridge.cpp").read_text(
                      encoding="utf-8")
        self.assertIn("ContextParameters->Location = Requests[0].SpawnContext.ImpactLocation;", bridge)
        self.assertIn("Requests.Num() > 1 && !Channel->IsA<UNiagaraDataChannel_Global>()", bridge)
        self.assertIn("MaxSharedBurstBatchSize = 128", bridge)
        self.assertNotIn("<<<<<<< HEAD", bridge)
        self.assertNotIn(">>>>>>>", bridge)

    def test_project_config_uses_distinct_category_and_real_experience_budget(self):
        """项目配置需使用专属类别、规范数组和真实Experience命名空间，静态通过仍非UE加载。"""
        report = MODULE.audit(ASSET_SPEC, ROOT)
        self.assertTrue(report["project_config_static_contract"], report["errors"])
        self.assertFalse(report["project_config_runtime_verified"])
        request_header = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/"
                          "Source/GamePlatformVFXClient/Public/Types/GamePlatformVFXRequest.h")
        provider = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/"
                    "Source/GamePlatformVFXClient/Private/Integration/Presentation/"
                    "GamePlatformVFXPresentationProvider.cpp")
        policy = (ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/"
                  "Source/GamePlatformVFXClient/Private/Scalability/"
                  "GamePlatformVFXScalabilityPolicy.cpp")
        self.assertIn("FName ExperienceId = NAME_None", request_header.read_text(encoding="utf-8"))
        self.assertIn("VFXRequest.ExperienceId = Request.Context.ExperienceId", provider.read_text(encoding="utf-8"))
        self.assertIn("bMatchExperience ? ExperienceId : WorldId", policy.read_text(encoding="utf-8"))

    def test_native_files_are_not_implicitly_verified(self):
        """仅检测文件名不能声称原生资产、Shader或GPU性能通过。"""
        report = MODULE.audit(ASSET_SPEC, ROOT)
        self.assertFalse(report["gpu_performance_verified"])
        for asset in report["native_files_found"] + report["native_files_missing"]:
            self.assertTrue(asset["path"].endswith(".uasset"))


if __name__ == "__main__":
    unittest.main()
