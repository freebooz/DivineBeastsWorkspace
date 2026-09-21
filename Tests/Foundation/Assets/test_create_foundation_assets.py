"""地图生成的离线回归；仅模拟引擎边界，不创建或冒充任何 UE 二进制资产。"""

import copy
import importlib.util
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import unittest
from unittest.mock import Mock, patch


SCRIPT = Path(__file__).resolve().parents[3] / "Tools/AssetTools/CreateFoundationAssets.py"
SPEC = importlib.util.spec_from_file_location("foundation_assets", SCRIPT)
ASSETS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ASSETS)

BOOTSTRAP = "/Game/Development/Foundation/Maps/L_FoundationBootstrap"
SANDBOX = "/Game/Development/Foundation/Maps/L_FoundationSandbox"
BOOTSTRAP_FILE = "Development/Foundation/Maps/L_FoundationBootstrap.umap"
SANDBOX_FILE = "Development/Foundation/Maps/L_FoundationSandbox.umap"


class MemoryEditor:
    """仅用于测试的引擎替身；磁盘证据在内存中模拟，无文件写入。"""

    def __init__(self, existing=()):
        self.files = {
            package.removeprefix("/Game/") + ".umap":
            {"size_bytes": 100, "sha256": "test-only-original", "mtime_ns": 1}
            for package in existing
        }
        self.registry = set(existing)
        self.created = []
        self.validated = []
        self.fail_validation = set()
        self.fail_creation = set()

    def snapshot(self):
        return copy.deepcopy(self.files)

    def exists(self, package):
        return package in self.registry

    def create(self, package):
        self.created.append(package)
        self.files[package.removeprefix("/Game/") + ".umap"] = {
            "size_bytes": 100, "sha256": "test-only-new", "mtime_ns": 2
        }
        self.registry.add(package)
        if package in self.fail_creation:
            raise RuntimeError("模拟保存空地图之后失败")

    def validate(self, package):
        self.validated.append(package)
        if package in self.fail_validation:
            raise RuntimeError("模拟已有地图缺少 PlayerStart")


class GenerationTests(unittest.TestCase):
    """防止覆盖、虚报成功、丢失部分失败以及越界生成回归。"""

    def test_creates_only_two_maps_and_second_run_only_validates(self):
        editor = MemoryEditor()
        first = ASSETS.run_maps(editor)
        self.assertEqual([item["state"] for item in first["maps"]], ["CREATED", "CREATED"])
        self.assertEqual(editor.created, [BOOTSTRAP, SANDBOX])
        self.assertEqual([item["path"] for item in first["native_asset_diff"]],
                         [BOOTSTRAP_FILE, SANDBOX_FILE])
        before = editor.snapshot()
        second = ASSETS.run_maps(editor)
        self.assertEqual([item["state"] for item in second["maps"]], ["VALIDATED", "VALIDATED"])
        self.assertEqual(editor.snapshot(), before)
        self.assertEqual(editor.created, [BOOTSTRAP, SANDBOX])
        self.assertEqual(second["native_asset_diff"], [])
        self.assertEqual(second["exit_code"], 0)

    def test_invalid_existing_map_is_failed_without_repair(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        editor.fail_validation.add(BOOTSTRAP)
        before = editor.snapshot()
        report = ASSETS.run_maps(editor)
        self.assertEqual([item["state"] for item in report["maps"]], ["FAILED", "VALIDATED"])
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual(editor.created, [])
        self.assertEqual(editor.snapshot(), before)

    def test_partial_creation_remains_failed_and_is_not_overwritten_next_run(self):
        editor = MemoryEditor()
        editor.fail_creation.add(BOOTSTRAP)
        first = ASSETS.run_maps(editor)
        self.assertEqual(first["maps"][0]["state"], "FAILED")
        self.assertIsNotNone(first["maps"][0]["after"])
        self.assertEqual(first["exit_code"], 1)
        editor.fail_validation.add(BOOTSTRAP)
        before = editor.snapshot()
        second = ASSETS.run_maps(editor)
        self.assertEqual(second["maps"][0]["state"], "FAILED")
        self.assertEqual(editor.created.count(BOOTSTRAP), 1)
        self.assertEqual(editor.snapshot(), before)

    def test_disk_asset_missing_from_registry_is_never_recreated(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        editor.registry.remove(BOOTSTRAP)
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(editor.created, [])

    def test_registry_only_asset_is_never_saved(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        del editor.files[BOOTSTRAP_FILE]
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(editor.created, [])

    def test_orphan_sidecars_and_wrong_asset_types_block_creation(self):
        for suffix in (".uasset", ".uexp", ".ubulk"):
            with self.subTest(suffix=suffix):
                editor = MemoryEditor((SANDBOX,))
                editor.files[BOOTSTRAP_FILE.removesuffix(".umap") + suffix] = {
                    "size_bytes": 10, "sha256": "test-only-orphan", "mtime_ns": 1
                }
                report = ASSETS.run_maps(editor)
                self.assertEqual(report["maps"][0]["state"], "FAILED")
                self.assertEqual(editor.created, [])

    def test_empty_package_cannot_be_validated(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        editor.files[BOOTSTRAP_FILE]["size_bytes"] = 0
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertNotIn(BOOTSTRAP, editor.validated)

    def test_create_success_without_persistent_package_is_failed(self):
        editor = MemoryEditor((SANDBOX,))
        editor.create = lambda package: editor.registry.add(package)
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(report["exit_code"], 1)

    def test_new_package_lost_after_validation_is_failed(self):
        editor = MemoryEditor()
        original_validate = editor.validate
        def delete_after_validation(package):
            original_validate(package)
            if package == SANDBOX:
                del editor.files[BOOTSTRAP_FILE]
        editor.validate = delete_after_validation
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(report["exit_code"], 1)

    def test_new_package_changed_after_validation_is_failed(self):
        editor = MemoryEditor()
        def change_after_first_validation(package):
            if package == SANDBOX:
                editor.files[BOOTSTRAP_FILE]["sha256"] = "changed-after-validation"
        editor.validate = change_after_first_validation
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(report["exit_code"], 1)

    def test_existing_native_asset_changes_are_reported_and_fail(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        def unexpected_change(package):
            editor.files[BOOTSTRAP_FILE]["sha256"] = "unexpected-change"
        editor.validate = unexpected_change
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(report["native_asset_diff"][0]["change"], "MODIFIED")

    def test_unlisted_native_asset_creation_is_failed(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        def unexpected_creation(package):
            editor.files["Unrequested.uasset"] = {
                "size_bytes": 20, "sha256": "unexpected-new", "mtime_ns": 2
            }
        editor.validate = unexpected_creation
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["exit_code"], 1)
        self.assertTrue(report["errors"])

    def test_orphan_external_actor_blocks_creation(self):
        editor = MemoryEditor((SANDBOX,))
        editor.files["__ExternalActors__/Development/Foundation/Maps/L_FoundationBootstrap/A/B.uasset"] = {
            "size_bytes": 10, "sha256": "test-only-external", "mtime_ns": 1
        }
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["maps"][0]["state"], "FAILED")
        self.assertEqual(editor.created, [])

    def test_final_audit_failure_is_not_reported_as_success(self):
        editor = MemoryEditor((BOOTSTRAP, SANDBOX))
        original_validate = editor.validate
        def audit_failure_after_last_map(package):
            original_validate(package)
            if package == SANDBOX:
                editor.snapshot = Mock(side_effect=OSError("模拟最终审计失败"))
        editor.validate = audit_failure_after_last_map
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["exit_code"], 1)
        self.assertFalse(report["audit_complete"])
        self.assertEqual([item["state"] for item in report["maps"]], ["FAILED", "FAILED"])

    def test_snapshot_failure_never_creates_assets(self):
        editor = MemoryEditor()
        editor.snapshot = Mock(side_effect=OSError("模拟只读审计失败"))
        report = ASSETS.run_maps(editor)
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual([item["state"] for item in report["maps"]], ["FAILED", "FAILED"])
        self.assertEqual(editor.created, [])

    def test_rejects_future_phases_and_unknown_arguments(self):
        for arguments in (["--phase", "Definitions"], ["--phase", "Flow"], ["--overwrite"]):
            with self.subTest(arguments=arguments):
                with self.assertRaises(ValueError):
                    ASSETS.parse_arguments(arguments)

    def test_missing_subsystem_fails_before_any_creation(self):
        unreal = Mock()
        unreal.SystemLibrary.get_command_line.return_value = (
            '-ExecutePythonScript="script.py --phase Maps" -unattended -ScriptErrorsAreFatal'
        )
        unreal.get_editor_subsystem.return_value = None
        with self.assertRaisesRegex(RuntimeError, "子系统"):
            ASSETS.UnrealMapEditor(unreal)


class EditorBoundaryTests(unittest.TestCase):
    """不能运行 UE 时仅验证引擎失败返回值的传播，不宣称模拟过真实序列化。"""

    def make_adapter(self):
        # 绕过真实编辑器构造，仅隔离创建／加载操作返回值；不产生磁盘文件。
        adapter = ASSETS.UnrealMapEditor.__new__(ASSETS.UnrealMapEditor)
        adapter.unreal = Mock()
        adapter.levels = Mock()
        adapter.actors = Mock()
        adapter.assets = Mock()
        adapter.assets.does_asset_exist.return_value = False
        adapter.game_mode = Mock()
        adapter.cube = Mock()
        adapter._world = Mock()
        adapter._validate_current = Mock()
        evidence = {BOOTSTRAP_FILE: {"size_bytes": 100, "sha256": "empty-map", "mtime_ns": 1}}
        adapter.snapshot = Mock(side_effect=[{}, evidence, evidence])
        return adapter

    def test_false_new_level_is_a_failure(self):
        adapter = self.make_adapter()
        adapter.levels.new_level.return_value = False
        with self.assertRaisesRegex(RuntimeError, "new_level"):
            adapter.create(BOOTSTRAP)
        adapter.levels.save_current_level.assert_not_called()

    def test_false_save_current_level_is_a_failure(self):
        adapter = self.make_adapter()
        adapter.levels.save_current_level.return_value = False
        with self.assertRaisesRegex(RuntimeError, "save_current_level"):
            adapter.create(BOOTSTRAP)
        adapter.levels.new_level.assert_called_once_with(BOOTSTRAP, is_partitioned_world=False)

    def test_failed_spawn_does_not_save_partial_scene(self):
        adapter = self.make_adapter()
        adapter.actors.spawn_actor_from_class.return_value = None
        with self.assertRaisesRegex(RuntimeError, "Actor"):
            adapter.create(BOOTSTRAP)
        adapter.levels.save_current_level.assert_not_called()

    def test_concurrent_disk_change_prevents_save(self):
        adapter = self.make_adapter()
        adapter.snapshot = Mock(side_effect=[{}, {BOOTSTRAP_FILE: "original"}, {BOOTSTRAP_FILE: "changed"}])
        with self.assertRaisesRegex(RuntimeError, "外部修改"):
            adapter.create(BOOTSTRAP)
        adapter.levels.save_current_level.assert_not_called()

    def test_false_load_level_is_a_failure_and_never_saves(self):
        adapter = self.make_adapter()
        adapter.levels.load_level.return_value = False
        with self.assertRaisesRegex(RuntimeError, "load_level"):
            adapter.validate(BOOTSTRAP)
        adapter.levels.save_current_level.assert_not_called()

    def test_transform_mismatch_and_nonfinite_values_fail(self):
        for x in (4.0, float("nan"), float("inf")):
            with self.subTest(x=x):
                with self.assertRaisesRegex(RuntimeError, "变换"):
                    ASSETS.UnrealMapEditor._check_components(Mock(x=x, y=0, z=0), (0, 0, 0),
                                                             ("x", "y", "z"), "地面")

    def test_commandlet_or_missing_fatal_flag_fails_before_subsystem_use(self):
        for command_line in (
            '-run=PythonScript -unattended -ScriptErrorsAreFatal',
            '-ExecutePythonScript="script.py" -unattended',
        ):
            with self.subTest(command_line=command_line):
                unreal = Mock()
                unreal.SystemLibrary.get_command_line.return_value = command_line
                with self.assertRaises(RuntimeError):
                    ASSETS.UnrealMapEditor(unreal)
                unreal.get_editor_subsystem.assert_not_called()

    def test_main_emits_failed_report_then_raises_on_unsupported_phase(self):
        output = io.StringIO()
        with redirect_stdout(output), self.assertRaises(RuntimeError):
            ASSETS.main(["--phase", "Definitions"])
        report = json.loads(output.getvalue().removeprefix("FOUNDATION_ASSETS_REPORT "))
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual([item["state"] for item in report["maps"]], ["FAILED", "FAILED"])


if __name__ == "__main__":
    unittest.main()
