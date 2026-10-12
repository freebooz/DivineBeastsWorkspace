#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""插件交付清单门禁的脚本回归：调用真实validate API，不启动UE或写真实清单。

临时目录仅放JSON描述、普通说明与路径夹具；证据存在不表示运行成功。
覆盖Passed身份/证据边界、Windows同名插件及真实Data场景入口，失败表示合同退化。
用法：python Tests/Foundation/Scripts/TestPluginDeliveryManifest.py。
"""
from __future__ import annotations

import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location(
    "plugin_delivery_manifest_under_test", ROOT / "Tests/Architecture/ValidatePluginDeliveryManifest.py")
API = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(API)


class ManifestBehaviorTests(unittest.TestCase):
    """使用独立的真实文件夹具；恢复旧的非空检查或字典折叠会使回归失败。"""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="plugin-manifest-test-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "workspace"
        self.root.mkdir()
        self.write("Game/Review.uproject", "{}")
        self.write("Game/Plugins/ContentPackRegistry.json", '{"ContentPacks":[]}')
        self.write("Build/Review.ps1", "# 仅供入口路径存在性夹具，不执行。\n")
        self.write("Saved/Review/evidence.txt", "这是普通路径夹具，不是UE运行报告。\n")
        descriptor = self.add_plugin("A", "ReviewPlugin")
        self.manifest = {
            "SchemaVersion": 1,
            "MainProject": "Game/Review.uproject",
            "ContentRegistry": "Game/Plugins/ContentPackRegistry.json",
            "ReservedModulePolicy": {"State": "Reserved", "ProductionCapability": False},
            "Plugins": [self.plugin_record("ReviewPlugin", descriptor)],
            "Scenarios": [{
                "Id": "review", "Entry": "Build/Review.ps1", "Profiles": ["FixtureOnly"],
                "Steps": ["只校验临时路径。"], "Expected": "不代表真实运行。",
                "Status": "Passed", "RunId": "edb4f598-7a9a-4e8e-ad4e-97d6bc7e344b",
                "Evidence": ["Saved/Review/evidence.txt"],
            }],
        }
        self.assertEqual(API.validate(self.root, self.manifest), [], "有效对照夹具必须先成立")

    def write(self, relative: str, text: str) -> Path:
        """仅在本测试拥有的临时根内建立普通文件，cleanup在用例退出时删除。"""
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def add_plugin(self, category: str, name: str) -> str:
        """返回真实描述的相对路径，用两个目录复现同名插件而不碰工作区插件。"""
        descriptor = f"Game/Plugins/{category}/{name}/{name}.uplugin"
        self.write(descriptor, json.dumps({"Modules": [{"Name": name + "Runtime", "Type": "Runtime"}]}))
        self.write(f"Game/Plugins/{category}/{name}/README.md", "测试描述夹具。\n")
        return descriptor

    @staticmethod
    def plugin_record(name: str, descriptor: str) -> dict:
        """登记与夹具描述一致的模块，避免无关漏项掩盖目标缺陷。"""
        return {
            "Id": name, "Descriptor": descriptor,
            "Readme": str(Path(descriptor).with_name("README.md")).replace("\\", "/"),
            "Kind": "Code", "Modules": [{
                "Name": name + "Runtime", "HostType": "Runtime", "Maturity": "SourcePresent"}],
            "Scenarios": ["review"],
        }

    def changed_scenario(self, **changes) -> dict:
        """每个负例只改变场景字段，真实清单和其他基线字段保持只读。"""
        manifest = copy.deepcopy(self.manifest)
        manifest["Scenarios"][0].update(changes)
        return manifest

    def assert_rejected(self, manifest: dict, diagnostic: str):
        """要求目标字段/身份的明确诊断，不能由其他偶发错误代替拒绝原因。"""
        errors = API.validate(self.root, manifest)
        self.assertTrue(any(diagnostic in error for error in errors), errors)

    def test_existing_evidence_checks_existence_only(self):
        # 普通文本和已有目录都可充当路径证据；门禁不解释它们为UE成功结果。
        manifest = self.changed_scenario(Evidence=["Saved/Review/evidence.txt", "Saved/Review"])
        self.assertEqual(API.validate(self.root, manifest), [])

    def test_passed_requires_guid_run_id(self):
        for run_id in (None, "", "not-a-run-id", 7, [], "edb4f598-7a9a-4e8e-ad4e-97d6bc7e344z"):
            with self.subTest(run_id=run_id):
                self.assert_rejected(self.changed_scenario(RunId=run_id), "RunId")

    def test_passed_requires_nonempty_string_list_evidence(self):
        for evidence in (None, [], "", "Saved/Review/evidence.txt", {}, [3], [""], ["Saved/Review/evidence.txt", None]):
            with self.subTest(evidence=evidence):
                self.assert_rejected(self.changed_scenario(Evidence=evidence), "Evidence")

    def test_passed_rejects_missing_evidence(self):
        self.assert_rejected(
            self.changed_scenario(Evidence=["Saved/Review/missing.txt"]), "Evidence")

    def test_passed_requires_canonical_relative_evidence_paths(self):
        for path in (
            r"Saved\Review\evidence.txt", "./Saved/Review/evidence.txt",
            "Saved//Review/evidence.txt", "Saved/Review/../Review/evidence.txt",
            "Saved/Review/evidence.txt/", str(self.root / "Saved/Review/evidence.txt"),
            "/Saved/Review/evidence.txt", "C:/Saved/Review/evidence.txt", "C:Saved/Review/evidence.txt",
        ):
            with self.subTest(path=path):
                self.assert_rejected(self.changed_scenario(Evidence=[path]), "Evidence")

    def test_resolved_evidence_cannot_leave_workspace(self):
        # 解析后仍须检查根边界；有文件的外部链接不能借用规范的相对别名。
        outside = Path(self.temporary.name) / "outside.txt"
        outside.write_text("测试根以外的证据夹具。\n", encoding="utf-8")
        link = self.root / "Saved/Review/outside.txt"
        try:
            link.symlink_to(outside)
        except (OSError, NotImplementedError) as error:
            self.skipTest(f"当前系统不能创建测试符号链接：{error}")
        self.assert_rejected(self.changed_scenario(Evidence=["Saved/Review/outside.txt"]), "Evidence")

    def test_nonpassed_scenarios_keep_nullable_run_fields(self):
        for state in ("NotRun", "Blocked", "Failed", "HistoricalOnly"):
            with self.subTest(state=state):
                manifest = self.changed_scenario(
                    Status=state, RunId=None, Evidence=[], BlockerZh="测试环境前置未满足。")
                self.assertEqual(API.validate(self.root, manifest), [])

    def test_main_project_rejects_single_backslashes(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["MainProject"] = r"Game\Review.uproject"
        self.assert_rejected(manifest, "MainProject")

    def test_duplicate_physical_plugin_basename_is_rejected(self):
        duplicate = self.add_plugin("B", "ReviewPlugin")
        self.manifest["Plugins"] = [self.plugin_record("ReviewPlugin", duplicate)]
        self.assert_rejected(self.manifest, "重名")

    def test_physical_plugin_collision_is_case_insensitive(self):
        duplicate = self.add_plugin("B", "reviewplugin")
        self.manifest["Plugins"].append(self.plugin_record("reviewplugin", duplicate))
        self.assert_rejected(self.manifest, "重名")


class ManifestScenarioBindingTests(unittest.TestCase):
    """真实清单必须把Data验收路由到包含新索引用例且要求构建证据的入口。"""

    def test_foundation_data_uses_current_entry_and_build_evidence(self):
        manifest = json.loads((ROOT / "Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json")
                              .read_text(encoding="utf-8-sig"))
        scenario = next(item for item in manifest["Scenarios"] if item["Id"] == "foundation-data")
        self.assertEqual(scenario["Entry"], "Build/Validation/TestPluginArchitectureUnreal.ps1")
        steps = "\n".join(scenario["Steps"])
        self.assertIn("-Suites Data", steps)
        self.assertIn("-BuildResultPath", steps)
        self.assertIn("16", scenario["Expected"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
