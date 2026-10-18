#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM全插件质量审查器离线回归：仅验证源集合/安全结论，不虚构UE/人工数据。"""
from __future__ import annotations

import copy
import importlib.util
import json
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"Tests/Architecture/AuditCurrentPluginQuality.py"
spec=importlib.util.spec_from_file_location("plugin_quality_audit",SOURCE)
assert spec and spec.loader
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PluginQualityAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.demo=module.load_json(module.DEMO)
        cls.delivery=module.load_json(module.DELIVERY)
        cls.report=module.audit(ROOT,cls.demo,cls.delivery)

    def test_67_stable_source_descriptors_and_names(self):
        self.assertEqual(67,self.report["pluginCount"])
        self.assertEqual(48,self.report["code"])
        self.assertEqual(19,self.report["content"])
        self.assertEqual(89,self.report["modules"])
        self.assertEqual([],self.report["issues"])
        self.assertEqual(67,len({p["pluginId"] for p in self.report["plugins"]}))

    def test_derived_demo_not_equal_to_human_approval(self):
        report=self.report
        self.assertEqual(4,report["demoFeatures"])
        self.assertEqual(9,report["demoScenarios"])
        self.assertEqual(0,report["humanApproved"])
        self.assertEqual("STATIC_INVENTORY_ONLY_NOT_ACCEPTANCE",report["state"])
        self.assertTrue(all(not p["humanApproved"] for p in report["plugins"]))

    def test_visual_plugins_need_actual_scenarios(self):
        self.assertGreater(self.report["visualRequired"],0)
        self.assertGreater(self.report["visualWithoutOperationalScenario"],0)
        cases=[p for p in self.report["plugins"] if p["visualReviewRequired"]
               and p["registeredScenarios"]==0]
        self.assertEqual(self.report["visualWithoutOperationalScenario"],len(cases))
        self.assertTrue(all(p["priority"]=="P1" for p in cases))
        self.assertIn("GamePlatformVFX",{p["pluginId"] for p in cases})
        self.assertIn("DBAWorldPack_Village",{p["pluginId"] for p in cases})

    def test_source_hotspots_are_review_candidates_not_proven_bottlenecks(self):
        self.assertGreater(sum(self.report["perfCandidates"].values()),0)
        self.assertIn("文本匹配只属于候选",module.markdown(self.report))
        self.assertNotIn("已经通过所有插件人工验收",module.markdown(self.report))

    def test_editor_only_asset_loading_not_called_runtime_hotpath(self):
        # DeveloperTools为Editor模块；SpatialDirector.LoadSynchronous受WITH_EDITOR保护。
        # 不把这两项当成已确认客户端/服务器同步加载。
        self.assertEqual(0,self.report["perfCandidates"].get("同步资源加载",0))
        for plugin in self.report["plugins"]:
            self.assertFalse(any(
                candidate["kind"] == "同步资源加载"
                for candidate in plugin["performanceReviewCandidates"]))

    def test_approved_diagnostic_shortcuts_are_real_and_not_formal_scenarios(self):
        """有地图入口不能当作功能完成；仅允许经过明确许可的两个资源挂载点。"""
        self.assertEqual(19, self.report["diagnosticShortcutPluginCount"])
        self.assertEqual(13, self.report["uniqueApprovedDiagnosticMapCount"])
        self.assertEqual(51, self.report["visualWithoutOperationalScenario"])
        model_path = (ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/"
                      "DivineBeastsDemoClient/Private/UI/DivineBeastsDemoHubViewModel.cpp")
        model = model_path.read_text(encoding="utf-8-sig")
        for plugin_id, package in module.DIAGNOSTIC_MAPS.items():
            with self.subTest(plugin=plugin_id):
                asset = module.physical_diagnostic_map(ROOT, package)
                self.assertTrue(module.valid_ue_map_header(asset))
                self.assertIn('PluginId == TEXT("' + plugin_id + '")', model)
                self.assertIn('Package == TEXT("' + package + '")', model)
                record = next(x for x in self.report["plugins"] if x["pluginId"] == plugin_id)
                self.assertEqual(package, record["diagnosticMapPackage"])
                self.assertFalse(record["diagnosticMapIsFormalScenario"])
        self.assertEqual(0, self.report["humanApproved"])

    def test_missing_demo_identity_cannot_pass(self):
        bad=copy.deepcopy(self.demo)
        bad["plugins"]=[p for p in bad["plugins"] if p["plugin_id"]!="GamePlatformVFX"]
        invalid=module.audit(ROOT,bad,self.delivery)
        self.assertTrue(any("编目" in x for x in invalid["issues"]))

    def test_markdown_has_one_row_per_real_plugin(self):
        doc=module.markdown(self.report)
        rows=[line for line in doc.splitlines() if line.startswith("| ")]
        # 标题行+67条插件；Markdown分隔符以|---开头，不计入|+空格匹配。
        self.assertEqual(67+1,len(rows))
        self.assertIn("人工签收0",doc)
        self.assertIn("功能总分母未逐项评审",doc)


if __name__=="__main__":
    unittest.main(verbosity=2)
