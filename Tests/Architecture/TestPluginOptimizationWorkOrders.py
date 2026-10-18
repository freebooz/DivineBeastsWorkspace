#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM插件优化工作单的静态回归。

测试真实67个稳定插件、89个模块与两份派生工作单设计是否一致，阻止空插件、
冒充已经操作的案例、虚假人工通过或伪造UE构建证据进入后续执行队列。
仅检验派生设计与磁盘来源，不启动UE，不生成蓝图或修改编目作者源。
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "Tests/Architecture/GeneratePluginOptimizationWorkOrders.py"
spec = importlib.util.spec_from_file_location("sslm_plugin_workorders", GENERATOR)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PluginOptimizationWorkOrdersTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.matrix, cls.source_bytes = module.read_source()
        cls.orders = module.build_workorders(cls.matrix, cls.source_bytes)
        cls.entries = cls.orders["workOrders"]

    def test_67_plugin_identities_remain_complete_without_duplicates(self):
        self.assertEqual(67, self.orders["totalPluginWorkOrders"])
        self.assertEqual(48, self.orders["codePluginWorkOrders"])
        self.assertEqual(19, self.orders["contentPackWorkOrders"])
        self.assertEqual(67, len({x["pluginId"] for x in self.entries}))
        self.assertEqual(set(x["pluginId"] for x in self.matrix["plugins"]),
                         set(x["pluginId"] for x in self.entries))
        self.assertTrue(all((ROOT / x["descriptorPath"]).is_file() for x in self.entries))

    def test_current_work_orders_cover_all_eight_stage_policy(self):
        self.assertEqual({"M1": 11, "M2": 9, "M3": 8, "M4": 7,
                          "M5": 13, "M6": 19}, self.orders["phaseCounts"])
        self.assertEqual("M0 核对全部当前身份与功能分母",
                         self.orders["globalPrerequisites"][0])
        self.assertIn("M7", self.orders["finalAcceptance"])
        self.assertEqual(89, sum(item["declaredModuleCount"] for item in self.entries))

    def test_19_content_packs_include_twelve_zodiac_heroes(self):
        packs = [x for x in self.entries if x["pluginKind"] == "content_pack"]
        heroes = [x for x in packs if x["pluginId"].startswith("DBAHeroPack_")]
        self.assertEqual(19, len(packs))
        self.assertEqual(12, len(heroes))
        self.assertTrue(all(x["primaryBatch"] == "M6" for x in packs))
        self.assertIn("子鼠", next(x for x in heroes
                                  if x["pluginId"] == "DBAHeroPack_Rat")["firstStepFocus"])

    def test_no_fake_manual_approval_or_production_claim(self):
        self.assertEqual(0, self.orders["humanApproved"])
        self.assertEqual(51, self.orders["formalScenarioGaps"])
        self.assertEqual(53, self.orders["visualReviewRequired"])
        self.assertEqual(9, self.orders["existingFormalScenarioRegistrations"])
        self.assertTrue(all(x["humanApproval"] == "not_verified" for x in self.entries))
        self.assertTrue(all(x["completionStatus"] == "not_implemented_by_this_plan"
                            for x in self.entries))
        self.assertTrue(all(x["runtimePerformanceStatus"] == "not_measured_by_this_plan"
                            for x in self.entries))
        self.assertTrue(all(not x["diagnosticIsFormalScenario"] for x in self.entries))
        self.assertIn("NOT_IMPLEMENTATION", self.orders["status"])

    def test_diagnostic_map_shortcuts_do_not_promote_formal_case(self):
        self.assertEqual(19, self.orders["diagnosticShortcutPlugins"])
        for item in self.entries:
            self.assertEqual(item["formalScenarioGap"],
                             item["visualReviewRequired"] and item["formalScenarioCount"] == 0)
            if item["existingDiagnosticMap"]:
                self.assertTrue(item["visualReviewRequired"])
                self.assertFalse(item["diagnosticIsFormalScenario"])
            self.assertEqual(5, len(item["tasks"]))
            self.assertEqual(5, len({task["id"] for task in item["tasks"]}))
            self.assertTrue(any("人工审核" in task["kind"] for task in item["tasks"]))

    def test_current_source_sha_and_stable_output_are_traceable(self):
        digest = hashlib.sha256(self.source_bytes).hexdigest()
        self.assertEqual(digest, self.orders["sourceSha256"])
        file_map_a = module.as_files()
        file_map_b = module.as_files()
        self.assertEqual(file_map_a, file_map_b)
        doc = file_map_a[module.OUTPUT_MD]
        self.assertIn("正式案例", doc)
        self.assertIn("诊断地图不作为正式可操作功能的完成证据", doc)
        self.assertIn("本工作单状态固定为未实施", doc)
        self.assertEqual(67, sum(1 for line in doc.splitlines()
                                 if line.startswith("| SSLM-PLG-")))

    def test_migration_remains_compatible_with_current_plugin_descriptors(self):
        for item in self.entries:
            with self.subTest(plugin=item["pluginId"]):
                self.assertEqual(item["workOrderId"], "SSLM-PLG-" + item["pluginId"])
                self.assertIn(item["priorityFromAudit"], ("P0", "P1", "P2", "P3"))
                self.assertTrue(all((ROOT / rule).is_file()
                                    for rule in item["moduleBuildRules"]))
        self.assertEqual(set(module.code_assignments()),
                         {x["pluginId"] for x in self.entries if x["pluginKind"] == "code"})

    def test_key_prior_remediations_remain_pending_runtime_evidence(self):
        for identity in ("GamePlatformSession", "GamePlatformTelemetry", "GamePlatformVFX",
                         "GamePlatformPCG", "DBAClient", "GamePlatformUI"):
            record = next(x for x in self.entries if x["pluginId"] == identity)
            self.assertTrue(record["alreadyChangedNotes"])
            self.assertEqual("not_implemented_by_this_plan", record["completionStatus"])
        self.assertTrue(all("当前Build.cs" in "\n".join(x["acceptanceEvidence"])
                            for x in self.entries))


if __name__ == "__main__":
    unittest.main(verbosity=2)
