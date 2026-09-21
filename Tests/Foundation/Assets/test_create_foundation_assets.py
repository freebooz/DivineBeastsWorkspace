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
PROBE = "/Game/Development/Foundation/Definitions/DA_FoundationProbe"
PROBE_FILE = "Development/Foundation/Definitions/DA_FoundationProbe.uasset"


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
        for arguments in (["--phase", "Definitions"], ["--overwrite"]):
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


class ProbeTests(unittest.TestCase):
    """探针阶段独立于地图；用内存引擎边界核验不覆盖与真实字段检查。"""

    def make_editor(self, existing=False):
        editor = MemoryEditor()
        if existing:
            editor.registry.add(PROBE)
            editor.files[PROBE_FILE] = {"size_bytes": 50, "sha256": "test-probe", "mtime_ns": 1}
        def create_probe(package):
            self.assertEqual(package, PROBE)
            editor.created.append(package)
            editor.registry.add(package)
            editor.files[PROBE_FILE] = {"size_bytes": 50, "sha256": "test-probe", "mtime_ns": 1}
        editor.create = create_probe
        return editor

    def test_probe_creates_only_requested_asset_and_then_validates(self):
        editor = self.make_editor()
        first = ASSETS.run_probe(editor)
        self.assertEqual(first["phase"], "Probe")
        self.assertEqual(first["assets"][0]["state"], "CREATED")
        self.assertEqual([item["path"] for item in first["native_asset_diff"]], [PROBE_FILE])
        second = ASSETS.run_probe(editor)
        self.assertEqual(second["assets"][0]["state"], "VALIDATED")
        self.assertEqual(second["exit_code"], 0)
        self.assertEqual(editor.created, [PROBE])
        self.assertEqual(second["native_asset_diff"], [])

    def test_probe_mismatch_never_repairs_existing_asset(self):
        editor = self.make_editor(existing=True)
        before = editor.snapshot()
        editor.fail_validation.add(PROBE)
        report = ASSETS.run_probe(editor)
        self.assertEqual(report["assets"][0]["state"], "FAILED")
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual(editor.files, before)
        self.assertEqual(editor.created, [])

    def test_probe_wrong_package_extension_blocks_creation(self):
        editor = self.make_editor()
        editor.files[PROBE_FILE.replace(".uasset", ".umap")] = {
            "size_bytes": 50, "sha256": "wrong-type", "mtime_ns": 1
        }
        report = ASSETS.run_probe(editor)
        self.assertEqual(report["assets"][0]["state"], "FAILED")
        self.assertEqual(editor.created, [])

    def test_probe_save_without_file_cannot_report_created(self):
        editor = self.make_editor()
        editor.create = lambda package: editor.registry.add(package)
        report = ASSETS.run_probe(editor)
        self.assertEqual(report["assets"][0]["state"], "FAILED")

    def test_probe_partial_failure_preserves_evidence(self):
        editor = self.make_editor()
        create_probe = editor.create
        def fail_after_creation(package):
            create_probe(package)
            raise RuntimeError("模拟探针保存失败")
        editor.create = fail_after_creation
        report = ASSETS.run_probe(editor)
        self.assertEqual(report["assets"][0]["state"], "FAILED")
        self.assertIsNotNone(report["assets"][0]["after"])
        self.assertEqual(report["native_asset_diff"][0]["path"], PROBE_FILE)

    def make_probe_adapter(self):
        adapter = ASSETS.UnrealProbeEditor.__new__(ASSETS.UnrealProbeEditor)
        adapter.unreal = Mock()
        adapter.assets = Mock()
        adapter.asset_tools = Mock()
        adapter.asset_class = object()
        adapter.assets.does_asset_exist.return_value = False
        adapter.snapshot = Mock(return_value={})
        return adapter

    def make_probe_asset(self, adapter):
        # 独立声明预期字段，不调用生产配置器生成验证夹具。
        logical_id = Mock()
        logical_id.get_editor_property.side_effect = {
            "namespace": "foundation", "name": "probe", "logical_version": 1
        }.__getitem__
        version = Mock()
        version.get_editor_property.side_effect = {"schema_version": 1, "content_revision": 1}.__getitem__
        asset = Mock()
        asset.get_class.return_value = adapter.asset_class
        asset.get_path_name.return_value = PROBE + ".DA_FoundationProbe"
        asset.get_editor_property.side_effect = {
            "logical_id": logical_id, "data_version": version,
            "required_definitions": [], "probe_value": 42,
        }.__getitem__
        return asset

    def test_probe_validates_every_authored_field(self):
        for field, bad_value in (("namespace", "other"), ("name", "wrong"), ("logical_version", 2),
                                 ("schema_version", 2), ("content_revision", 0),
                                 ("required_definitions", ["dependency"])):
            with self.subTest(field=field):
                adapter = self.make_probe_adapter()
                asset = self.make_probe_asset(adapter)
                ASSETS.UnrealProbeEditor._validate_object(adapter, asset)
                owner = asset
                if field in ("namespace", "name", "logical_version"):
                    owner = asset.get_editor_property("logical_id")
                elif field in ("schema_version", "content_revision"):
                    owner = asset.get_editor_property("data_version")
                original_get = owner.get_editor_property.side_effect
                owner.get_editor_property.side_effect = lambda key: bad_value if key == field else original_get(key)
                with self.assertRaises(RuntimeError):
                    adapter._validate_object(asset)

    def test_probe_wrong_reflected_class_or_redirect_is_rejected(self):
        for wrong_class in (True, False):
            adapter = self.make_probe_adapter()
            asset = self.make_probe_asset(adapter)
            if wrong_class:
                asset.get_class.return_value = object()
            else:
                asset.get_path_name.return_value = "/Game/Other.Other"
            with self.assertRaises(RuntimeError):
                adapter._validate_object(asset)

    def test_missing_probe_reflection_fails_before_factory_creation(self):
        unreal = Mock()
        unreal.load_class.side_effect = [object(), None]
        with patch.object(ASSETS.UnrealAssetEditor, "__init__", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "反射"):
                ASSETS.UnrealProbeEditor(unreal)
        unreal.AssetToolsHelpers.get_asset_tools.assert_not_called()

    def test_probe_false_save_is_failure(self):
        adapter = self.make_probe_adapter()
        adapter.asset_tools.create_asset.return_value = self.make_probe_asset(adapter)
        adapter.assets.save_loaded_asset.return_value = False
        adapter._validate_object = Mock(return_value=[])
        with self.assertRaisesRegex(RuntimeError, "save_loaded_asset"):
            adapter.create(PROBE)
        self.assertFalse(adapter.asset_tools.create_asset.call_args.kwargs["overwrite_existing"])

    def test_custom_probe_value_is_valid_and_never_reset(self):
        adapter = self.make_probe_adapter()
        asset = self.make_probe_asset(adapter)
        original_get = asset.get_editor_property.side_effect
        asset.get_editor_property.side_effect = lambda key: 99 if key == "probe_value" else original_get(key)
        differences = adapter._validate_object(asset)
        self.assertIn({"field": "probe_value", "default": 42, "actual": 99}, differences)
        asset.set_editor_property.assert_not_called()

    def test_probe_failed_reload_and_wrong_primary_id_tags_fail(self):
        adapter = self.make_probe_adapter()
        asset = self.make_probe_asset(adapter)
        adapter.assets.load_asset.return_value = asset
        adapter.unreal.EditorLoadingAndSavingUtils.reload_packages.return_value = (False, "模拟重载失败")
        with self.assertRaisesRegex(RuntimeError, "重载"):
            adapter.validate(PROBE)
        adapter.unreal.EditorLoadingAndSavingUtils.reload_packages.return_value = (True, "")
        adapter.assets.get_tag_values.return_value = {
            "PrimaryAssetType": "GamePlatformDefinition", "PrimaryAssetName": "foundation.probe@1",
            "GamePlatformLogicalId": "foundation.probe@1",
        }
        adapter.validate(PROBE)
        for key in ("PrimaryAssetType", "PrimaryAssetName", "GamePlatformLogicalId"):
            original = adapter.assets.get_tag_values.return_value[key]
            adapter.assets.get_tag_values.return_value[key] = "wrong"
            with self.assertRaisesRegex(RuntimeError, key):
                adapter.validate(PROBE)
            adapter.assets.get_tag_values.return_value[key] = original
        adapter.assets.save_loaded_asset.assert_not_called()

    def test_main_probe_initialization_failure_reports_probe_not_maps(self):
        output = io.StringIO()
        with patch.dict("sys.modules", {"unreal": Mock()}), \
                patch.object(ASSETS, "UnrealProbeEditor", side_effect=RuntimeError("反射缺失")), \
                redirect_stdout(output), self.assertRaises(RuntimeError):
            ASSETS.main(["--phase", "Probe"])
        report = json.loads(output.getvalue().removeprefix("FOUNDATION_ASSETS_REPORT "))
        self.assertEqual(report["phase"], "Probe")
        self.assertEqual(report["assets"][0]["package"], PROBE)
        self.assertEqual(report["assets"][0]["state"], "FAILED")


class FlowTests(unittest.TestCase):
    """生成默认路径与合法用户编辑分离；错误图拒绝，既有值不回写。"""

    def test_flow_defaults_have_explicit_probe_request_and_timeouts(self):
        values = ASSETS.default_flow_values()
        self.assertEqual(values["entry_node_id"], "Boot")
        self.assertEqual([node["node_id"] for node in values["nodes"]],
                         ["Boot", "ValidateConfiguration", "LoadProbeDefinition", "EnterSandbox", "Ready"])
        self.assertEqual([node["timeout_seconds"] for node in values["nodes"]], [30, 30, 30, 120, 30])
        self.assertEqual([node["input_definition_id"] for node in values["nodes"]],
                         ["", "", "GamePlatformDefinition:foundation.probe@1", "", ""])
        self.assertEqual(values["nodes"][-1]["next_node_id"], "None")
        self.assertEqual(ASSETS.validate_flow_values(values), [])

    def test_legal_custom_routes_and_timeout_are_reported_without_reset(self):
        values = ASSETS.default_flow_values()
        values["nodes"][0]["next_node_id"] = "None"
        values["nodes"][0]["routes"] = {"Continue": "ValidateConfiguration"}
        values["nodes"][1]["timeout_seconds"] = 45
        before = copy.deepcopy(values)
        differences = ASSETS.validate_flow_values(values)
        self.assertTrue(differences)
        self.assertEqual(values, before)

    def test_invalid_graph_is_rejected(self):
        for failure in ("duplicate", "dangling", "unreachable", "entry", "timeout", "nan",
                        "executor", "route", "cycle", "budget", "input"):
            with self.subTest(failure=failure):
                values = ASSETS.default_flow_values()
                node = values["nodes"][0]
                if failure == "duplicate": values["nodes"][1]["node_id"] = "boot"
                elif failure == "dangling": node["next_node_id"] = "Missing"
                elif failure == "unreachable": node["next_node_id"] = "Ready"
                elif failure == "entry": values["entry_node_id"] = "Missing"
                elif failure == "timeout": node["timeout_seconds"] = 0
                elif failure == "nan": node["timeout_seconds"] = float("nan")
                elif failure == "executor": node["executor_id"] = "None"
                elif failure == "route": node["routes"] = {"None": "Ready"}
                elif failure == "cycle": values["nodes"][-1]["next_node_id"] = "Boot"
                elif failure == "budget": values["max_immediate_cycle_transitions"] = 0
                elif failure == "input": node["input_definition_id"] = "Other:foundation.probe@1"
                with self.assertRaises(RuntimeError):
                    ASSETS.validate_flow_values(values)

    def test_explicit_bounded_cycle_is_valid_customization(self):
        values = ASSETS.default_flow_values()
        values["allow_cycles"] = True
        values["nodes"][-1]["next_node_id"] = "Boot"
        self.assertTrue(ASSETS.validate_flow_values(values))

    def test_flow_stage_only_creates_flow_and_propagates_custom_differences(self):
        package = "/Game/Development/Foundation/Definitions/DA_FoundationFlow"
        filename = "Development/Foundation/Definitions/DA_FoundationFlow.uasset"
        editor = MemoryEditor()
        def create_flow(requested):
            self.assertEqual(requested, package)
            editor.created.append(requested)
            editor.registry.add(requested)
            editor.files[filename] = {"size_bytes": 100, "sha256": "test-flow", "mtime_ns": 1}
        editor.create = create_flow
        editor.validate = lambda requested: [{"field": "nodes", "default": "default-path", "actual": "custom-path"}]
        first = ASSETS.run_flow(editor)
        self.assertEqual(first["assets"][0]["state"], "CREATED")
        self.assertEqual(editor.created, [package])
        second = ASSETS.run_flow(editor)
        self.assertEqual(second["assets"][0]["state"], "VALIDATED")
        self.assertTrue(second["assets"][0]["custom_differences"])
        self.assertEqual(second["native_asset_diff"], [])

    def test_flow_factory_writes_real_fields_and_no_required_dependencies(self):
        class ReflectedFields:
            """仅测试使用的反射属性存储，所有状态在内存，不生成资产字节。"""
            def __init__(self, **fields):
                self.fields = fields
            def get_editor_property(self, name):
                return self.fields[name]
            def set_editor_property(self, name, value):
                self.fields[name] = value

        adapter = ASSETS.UnrealFlowEditor.__new__(ASSETS.UnrealFlowEditor)
        adapter.unreal = Mock()
        adapter.unreal.GamePlatformFlowNodeDefinition.side_effect = ReflectedFields
        adapter.unreal.PrimaryAssetType.side_effect = lambda: ReflectedFields(name="None")
        adapter.unreal.PrimaryAssetId.side_effect = lambda: ReflectedFields(
            primary_asset_type=ReflectedFields(name="None"), primary_asset_name="None")
        adapter.assets = Mock()
        adapter.assets.does_asset_exist.return_value = False
        adapter.asset_tools = Mock()
        adapter.asset_class = object()
        adapter.snapshot = Mock(return_value={})
        asset = ReflectedFields(
            logical_id=ReflectedFields(namespace="", name="", logical_version=1),
            data_version=ReflectedFields(schema_version=1, content_revision=1))
        asset.get_class = lambda: adapter.asset_class
        asset.get_path_name = lambda: "/Game/Development/Foundation/Definitions/DA_FoundationFlow.DA_FoundationFlow"
        adapter.asset_tools.create_asset.return_value = asset
        adapter.create("/Game/Development/Foundation/Definitions/DA_FoundationFlow")
        self.assertEqual(asset.fields["required_definitions"], [])
        self.assertEqual(asset.fields["logical_id"].fields, {"namespace": "foundation", "name": "flow", "logical_version": 1})
        self.assertEqual(asset.fields["entry_node_id"], "Boot")
        nodes = asset.fields["nodes"]
        self.assertEqual([node.fields["executor_id"] for node in nodes],
                         ["Boot", "ValidateConfiguration", "LoadProbeDefinition", "EnterSandbox", "Ready"])
        probe_id = nodes[2].fields["input_definition_id"]
        self.assertEqual(probe_id.fields["primary_asset_type"].fields["name"], "GamePlatformDefinition")
        self.assertEqual(probe_id.fields["primary_asset_name"], "foundation.probe@1")
        for index in (0, 1, 3, 4):
            self.assertEqual(nodes[index].fields["input_definition_id"].fields["primary_asset_name"], "None")
        self.assertEqual(adapter._validate_object(asset), [])
        nodes[0].fields["routes"] = {"Optional": "Ready"}
        self.assertTrue(adapter._validate_object(asset))
        asset.fields["required_definitions"] = [probe_id]
        with self.assertRaisesRegex(RuntimeError, "RequiredDefinitions"):
            adapter._validate_object(asset)
        adapter.assets.load_asset.assert_not_called()

    def test_missing_flow_reflection_does_not_load_probe_or_create_factory(self):
        unreal = Mock()
        unreal.load_class.side_effect = [object(), None]
        with patch.object(ASSETS.UnrealAssetEditor, "__init__", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "反射"):
                ASSETS.UnrealFlowEditor(unreal)
        self.assertEqual(unreal.load_class.call_args.args[1], "/Script/GamePlatformApplicationFlow.GamePlatformFlowDefinition")
        unreal.AssetToolsHelpers.get_asset_tools.assert_not_called()

    def test_new_definition_must_match_defaults_even_though_existing_customization_is_valid(self):
        adapter = ASSETS.UnrealProbeEditor.__new__(ASSETS.UnrealProbeEditor)
        adapter.unreal = Mock()
        adapter.assets = Mock()
        adapter.assets.does_asset_exist.return_value = False
        adapter.asset_tools = Mock()
        adapter.asset_class = object()
        adapter.snapshot = Mock(return_value={})
        adapter._validate_identity = Mock()
        adapter._validate_object = Mock(return_value=[{"field": "probe_value", "default": 42, "actual": 99}])
        with self.assertRaisesRegex(RuntimeError, "默认"):
            adapter.create(PROBE)
        adapter.assets.save_loaded_asset.assert_not_called()


class OnlineFlowTests(unittest.TestCase):
    """在线开发流程必须在真实认证和资料读取后才接入既有数据/地图链，不能更改单机默认图。"""

    def test_online_flow_is_explicit_and_preserves_standalone(self):
        standalone = ASSETS.default_flow_values()
        values = ASSETS.default_online_flow_values()
        self.assertEqual([node["executor_id"] for node in values["nodes"]],
                         ["OnlineValidateConfiguration", "OnlineProbeService", "OnlineLogin", "OnlineReadProfile",
                          "LoadProbeDefinition", "EnterSandbox", "OnlineReady"])
        self.assertEqual(ASSETS.default_flow_values(), standalone)
        self.assertEqual(ASSETS.validate_flow_values(values, values), [])

    def test_online_phase_has_single_separate_asset_and_no_overwrite(self):
        self.assertEqual(ASSETS.parse_arguments(["--phase", "OnlineFlow"]).phase, "OnlineFlow")
        report = ASSETS.failed_report("UE未执行", "OnlineFlow")
        self.assertEqual([item["package"] for item in report["assets"]],
                         ["/Game/Development/Foundation/Definitions/DA_FoundationOnlineFlow"])
        self.assertEqual(ASSETS.asset_filename(ASSETS.ONLINE_FLOW_PACKAGE),
                         "Development/Foundation/Definitions/DA_FoundationOnlineFlow.uasset")

    def test_online_definition_never_contains_project_classes_maps_or_credentials(self):
        values = ASSETS.default_online_flow_values()
        serialized = str(values)
        self.assertNotIn("/Script/DivineBeastsArena", serialized)
        self.assertNotIn("/Game/", serialized)
        self.assertNotIn("password", serialized.lower())
        inputs = [node["input_definition_id"] for node in values["nodes"] if node["input_definition_id"]]
        self.assertEqual(inputs, ["GamePlatformDefinition:foundation.probe@1"])


if __name__ == "__main__":
    unittest.main()
