"""离线测试真实生成策略；编辑器替身仅模拟存储，不生成或冒充 UE 资产。"""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "Tools/AssetTools"))
SCRIPT = ROOT / "Tools/AssetTools/CreateWorldAssets.py"


class MemoryEditor:
    """仅用于本文件：让生产审计算法观察保存、占用及异常写入。"""
    def __init__(self, module):
        self.module = module
        self.files = {module.MAP_FILE: {"size_bytes": 8, "sha256": "original", "mtime_ns": 1}}
        self.registered = set()
        self.values = {}
        self.calls = []
        self.fail = None
        self.mutate = False
        self.extra_write = False
        self.skip_save = False

    def snapshot(self):
        return copy.deepcopy(self.files)

    def preflight(self):
        if self.module.MAP_FILE not in self.files:
            raise RuntimeError("地图缺失")

    def exists(self, package):
        return package in self.registered

    def create(self, package):
        self.calls.append(package)
        if package == self.fail:
            raise RuntimeError("保存失败")
        self.registered.add(package)
        self.values[package] = self.module.default_values(package)
        if not self.skip_save:
            self.files[self.module.filename(package)] = {"size_bytes": 20, "sha256": package, "mtime_ns": 2}
        if self.extra_write:
            self.files["Unrelated.uasset"] = {"size_bytes": 4, "sha256": "unexpected", "mtime_ns": 3}

    def validate(self, package):
        if self.mutate:
            self.files[self.module.filename(package)]["mtime_ns"] += 1
        return self.module.validate_values(package, self.values[package])


class WorldAssetTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(SCRIPT.is_file(), "尚未实现 World 资产生成器")
        spec = importlib.util.spec_from_file_location("world_assets", SCRIPT)
        self.module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.module)
        self.editor = MemoryEditor(self.module)

    def run_assets(self):
        return self.module.run_assets(self.editor)

    def test_create_regions_before_world_and_audit_map_unchanged(self):
        original = self.editor.snapshot()
        report = self.run_assets()
        self.assertEqual(report["exit_code"], 0)
        self.assertEqual(self.editor.calls, list(self.module.PACKAGES))
        self.assertEqual([x["state"] for x in report["assets"]], ["CREATED"] * 3)
        self.assertEqual(self.editor.files[self.module.MAP_FILE], original[self.module.MAP_FILE])
        world = self.editor.values[self.module.WORLD_PACKAGE]
        self.assertEqual(world["regions"], ["GamePlatformDefinition:foundation.region_a@1", "GamePlatformDefinition:foundation.region_b@1"])
        self.assertEqual(world["required_definitions"], world["regions"])

    def test_existing_custom_timeout_is_reported_never_saved(self):
        self.assertEqual(self.run_assets()["exit_code"], 0)
        self.editor.calls.clear()
        self.editor.values[self.module.WORLD_PACKAGE]["readiness_timeout_seconds"] = 75
        report = self.run_assets()
        self.assertEqual(report["exit_code"], 0)
        self.assertEqual(self.editor.calls, [])
        self.assertEqual(report["assets"][-1]["custom_differences"][0]["actual"], 75)

    def test_missing_map_rejects_before_any_creation(self):
        self.editor.files.clear()
        self.assertEqual(self.run_assets()["exit_code"], 1)
        self.assertEqual(self.editor.calls, [])

    def test_orphan_sidecar_is_not_overwritten(self):
        package = self.module.PACKAGES[0]
        self.editor.files[self.module.filename(package).replace(".uasset", ".uexp")] = {"size_bytes": 2}
        self.assertEqual(self.run_assets()["exit_code"], 1)
        self.assertNotIn(package, self.editor.calls)

    def test_registry_without_disk_is_not_overwritten(self):
        self.editor.registered.add(self.module.PACKAGES[0])
        self.assertEqual(self.run_assets()["exit_code"], 1)
        self.assertEqual(self.editor.calls, [])

    def test_failed_region_prevents_world_and_preserves_first_asset(self):
        self.editor.fail = self.module.PACKAGES[1]
        self.assertEqual(self.run_assets()["exit_code"], 1)
        self.assertNotIn(self.module.WORLD_PACKAGE, self.editor.calls)
        self.assertIn(self.module.filename(self.module.PACKAGES[0]), self.editor.files)

    def test_claimed_save_without_disk_fails(self):
        self.editor.skip_save = True
        self.assertEqual(self.run_assets()["exit_code"], 1)

    def test_readonly_validation_mutation_fails(self):
        self.assertEqual(self.run_assets()["exit_code"], 0)
        self.editor.mutate = True
        self.assertEqual(self.run_assets()["exit_code"], 1)

    def test_unrelated_native_write_fails_audit(self):
        self.editor.extra_write = True
        report = self.run_assets()
        self.assertEqual(report["exit_code"], 1)
        self.assertTrue(report["audit_complete"])
        self.assertTrue(report["errors"])

    def test_identity_dependency_map_and_numeric_constraints(self):
        package = self.module.WORLD_PACKAGE
        for field, bad in (("logical_id", "foundation.other@1"), ("schema_version", 2),
                           ("content_revision", 0), ("map_identity", "/Game/Other.Other"),
                           ("regions", []), ("required_definitions", []),
                           ("readiness_timeout_seconds", float("nan")), ("readiness_timeout_seconds", 0)):
            with self.subTest(field=field, bad=bad):
                values = self.module.default_values(package)
                values[field] = bad
                with self.assertRaises(RuntimeError):
                    self.module.validate_values(package, values)

    def test_region_policies_and_parent_are_validated(self):
        for field, bad in (("bounds_policy", "Sphere"), ("activation_policy", "Hidden"),
                           ("region_type_tag", ""), ("parent_region_id", "foundation.region_a@1")):
            values = self.module.default_values(self.module.PACKAGES[0])
            values[field] = bad
            with self.assertRaises(RuntimeError):
                self.module.validate_values(self.module.PACKAGES[0], values)

    def test_unknown_package_cannot_escape_whitelist(self):
        with self.assertRaises(RuntimeError):
            self.module.filename("/Game/../../Other")


if __name__ == "__main__":
    unittest.main()
