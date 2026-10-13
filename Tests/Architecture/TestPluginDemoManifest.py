"""SSLM演示编目离线回归（项目测试层，仅标准库）。

验证身份关系、四维状态和拒绝路径，不加载UE，不把测试夹具当成已交付场景。
调用方为开发者/静态门禁；只读真实插件描述，所有变异仅作用于深复制的内存夹具。
"""
from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import unittest


WORKSPACE = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = Path(__file__).with_name("ValidatePluginDemoManifest.py")


def make_manifest() -> dict:
    """独立构造合法的开发夹具；描述来源真实，但案例本身只用于验证器回归。"""
    plugins = []
    for directory, layer in (("GamePlatform", "Platform"), ("MobaCommon", "Moba"),
                             ("DivineBeasts", "Project")):
        for path in sorted((WORKSPACE / "Game/Plugins" / directory).rglob("*.uplugin")):
            description = json.loads(path.read_text(encoding="utf-8-sig"))
            plugins.append({"plugin_id": path.stem, "descriptor_path": path.relative_to(WORKSPACE).as_posix(),
                            "architecture_layer": layer,
                            "plugin_kind": "code" if description.get("Modules") else "content_pack",
                            "feature_audit_status": "unreviewed"})
    return {"schema_version": 2, "plugins": plugins,
            "features": [{"feature_id": "GamePlatformUI.ResourceBar", "plugin_id": "GamePlatformUI",
                          "title": "资源条（验证夹具）", "purpose": "验证开发数值投影",
                          "implementation_status": "implemented", "integration_status": "registered",
                          "run_status": "idle", "review_status": "not_reviewed",
                          "owner_path": "Game/Plugins/GamePlatform/Presentation/GamePlatformUI",
                          "implementation_evidence": [{"path": "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/README.md",
                                                       "symbol": "资源条夹具：非源码审计证明"}],
                          "scenario_ids": ["GamePlatformUI.ResourceBar.Normal"]}],
            "scenarios": [{"scenario_id": "GamePlatformUI.ResourceBar.Normal", "feature_id": "GamePlatformUI.ResourceBar",
                           "title": "正常值", "related_feature_ids": [], "adapter_id": "ResourceBar",
                           "template": "UIWidgetGallery", "run_mode": "fixture",
                           "targets": ["Editor", "Client"], "required_plugins": ["GamePlatformUI"],
                           "preconditions": ["仅供回归"], "map_asset": "/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub",
                           "widget_asset": "", "definition_assets": [], "operations": ["Execute", "Stop", "Reset"],
                           "prepare_timeout_seconds": 10, "stop_timeout_seconds": 10, "parameters": [],
                           "review_case_ids": ["GamePlatformUI.ResourceBar.Normal.Review"]}],
            "review_cases": [{"review_case_id": "GamePlatformUI.ResourceBar.Normal.Review", "version": 1,
                              "scenario_id": "GamePlatformUI.ResourceBar.Normal", "feature_id": "GamePlatformUI.ResourceBar",
                              "steps": ["提交50/100"], "expected_result": "比例为0.5", "required_evidence": ["log"]}],
            "legacy_map_routes": [], "discovered_maps": []}


class PluginDemoManifestTests(unittest.TestCase):
    """各变异应产生具名拒绝；删除生产验证分支将使相应断言失败。"""

    @classmethod
    def setUpClass(cls):
        """只加载真实验证器；实现尚缺时由断言给出红测，不用假实现代替。"""
        cls.validator = None
        if VALIDATOR_PATH.is_file():
            spec = importlib.util.spec_from_file_location("plugin_demo_manifest", VALIDATOR_PATH)
            cls.validator = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(cls.validator)

    def validate(self, document):
        """返回真实诊断；缺实现属于明确测试失败，不吞掉为成功。"""
        self.assertIsNotNone(self.validator, "尚未实现编目验证器")
        return self.validator.validate_manifest(document, WORKSPACE)

    def assert_issue(self, document, code):
        """检查稳定错误码，不绑定可润色的中文错误正文。"""
        self.assertIn(code, {issue["code"] for issue in self.validate(document)})

    def test_valid_manifest(self):
        """合法关系不应被误拒绝，尤其内容包仍属Project层。"""
        self.assertEqual([], self.validate(make_manifest()))

    def test_duplicate_feature_rejected(self):
        """FName身份大小写折叠后重复也必须拒绝。"""
        document = make_manifest()
        duplicate = copy.deepcopy(document["features"][0])
        duplicate["feature_id"] = duplicate["feature_id"].lower()
        document["features"].append(duplicate)
        self.assert_issue(document, "duplicate_id")

    def test_two_scenarios_share_map(self):
        """同图不同案例合法，各自有独立用例，不能按地图去重。"""
        document = make_manifest()
        second = copy.deepcopy(document["scenarios"][0])
        second["scenario_id"] = "GamePlatformUI.ResourceBar.InvalidMaximum"
        second["review_case_ids"] = [second["scenario_id"] + ".Review"]
        review = copy.deepcopy(document["review_cases"][0])
        review.update(review_case_id=second["review_case_ids"][0], scenario_id=second["scenario_id"])
        document["scenarios"].append(second)
        document["review_cases"].append(review)
        document["features"][0]["scenario_ids"].append(second["scenario_id"])
        self.assertEqual([], self.validate(document))

    def test_unreviewed_is_not_complete(self):
        """功能分母未知时不得输出100%，已知功能数仍可展示。"""
        self.validate(make_manifest())
        summary = self.validator.summarize_coverage(make_manifest())
        self.assertIsNone(summary["coverage_percent"])
        self.assertEqual(1, summary["known_features"])
        self.assertEqual(0, summary["human_approved_features"])

    def test_missing_plugin_visible(self):
        """真实存在但清单漏列的插件产生具名缺口，不默默忽略。"""
        document = make_manifest()
        missing = document["plugins"].pop()["plugin_id"]
        issues = self.validate(document)
        self.assertTrue(any(issue["code"] == "plugin_missing" and missing in issue["message"] for issue in issues))

    def test_legacy_map_ambiguity(self):
        """旧路径一对多不能设置自动首选；要求人工选择时允许。"""
        document = make_manifest()
        second = copy.deepcopy(document["scenarios"][0])
        second["scenario_id"] = "GamePlatformUI.ResourceBar.Cancel"
        second["review_case_ids"] = [second["scenario_id"] + ".Review"]
        review = copy.deepcopy(document["review_cases"][0])
        review.update(review_case_id=second["review_case_ids"][0], scenario_id=second["scenario_id"])
        document["scenarios"].append(second)
        document["review_cases"].append(review)
        document["features"][0]["scenario_ids"].append(second["scenario_id"])
        route = {"map_asset": document["scenarios"][0]["map_asset"],
                 "scenario_ids": [document["scenarios"][0]["scenario_id"], second["scenario_id"]],
                 "requires_selection": False}
        document["legacy_map_routes"] = [route]
        self.assert_issue(document, "legacy_route_ambiguous")
        route["requires_selection"] = True
        self.assertEqual([], self.validate(document))

    def test_unknown_scenario_reference(self):
        """功能声明不存在的案例，必须阻止发布半目录。"""
        document = make_manifest()
        document["features"][0]["scenario_ids"] = ["Missing.Case"]
        self.assert_issue(document, "reference_missing")

    def test_wrong_target_rejected(self):
        """开发客户端菜单不得声明Server/Shipping为可执行目标。"""
        for target in ("Server", "Shipping"):
            with self.subTest(target=target):
                document = make_manifest()
                document["scenarios"][0]["targets"] = [target]
                self.assert_issue(document, "invalid_value")

    def test_missing_review_evidence_rejected(self):
        """无必需证据的用例不能成为人工审核合同。"""
        document = make_manifest()
        document["review_cases"][0]["required_evidence"] = []
        self.assert_issue(document, "required_value")

    def test_timeout_must_be_finite_positive(self):
        """零、负、无穷、NaN、布尔值都不能充当秒数。"""
        for value in (0, -1, float("nan"), float("inf"), True):
            with self.subTest(value=value):
                document = make_manifest()
                document["scenarios"][0]["prepare_timeout_seconds"] = value
                self.assert_issue(document, "invalid_value")

    def test_path_escape_rejected(self):
        """审计依据只允许仓库内常规路径，不读取路径外文件。"""
        document = make_manifest()
        document["features"][0]["implementation_evidence"][0]["path"] = "../AGENTS.md"
        self.assert_issue(document, "unsafe_path")

    def test_malformed_records_report_instead_of_crashing(self):
        """错误类型必须以诊断返回，不能因字典访问异常让门禁失效。"""
        for collection in ("plugins", "features", "scenarios", "review_cases", "legacy_map_routes"):
            with self.subTest(collection=collection):
                document = make_manifest()
                document[collection] = [None]
                self.assert_issue(document, "invalid_type")

    def test_validation_does_not_mutate_source(self):
        """检查函数没有作者写入权限，原始失败和状态不得被修正掉。"""
        document = make_manifest()
        original = copy.deepcopy(document)
        self.validate(document)
        self.assertEqual(original, document)

    def test_oversized_numeric_input_reports_error(self):
        """超大JSON整数不可导致有限数检查溢出崩溃。"""
        document = make_manifest()
        document["scenarios"][0]["prepare_timeout_seconds"] = 10 ** 400
        self.assert_issue(document, "invalid_value")

    def test_source_evidence_requires_a_file(self):
        """目录存在不能冒充实际代码依据文件。"""
        document = make_manifest()
        document["features"][0]["implementation_evidence"][0]["path"] = "Game/Plugins"
        self.assert_issue(document, "evidence_not_file")

    def test_owner_plugin_must_be_a_required_dependency(self):
        """案例不能省略主功能所有者，避免运行预检绕过禁用插件。"""
        document = make_manifest()
        document["scenarios"][0]["required_plugins"] = ["GamePlatformCore"]
        self.assert_issue(document, "owner_dependency_missing")

    def test_content_is_not_a_fourth_layer(self):
        """内容插件仍属项目层，历史Content值必须在迁移时消除。"""
        document = make_manifest()
        next(item for item in document["plugins"] if item["plugin_kind"] == "content_pack")["architecture_layer"] = "Content"
        self.assert_issue(document, "invalid_value")

    def test_author_cannot_claim_human_passed(self):
        """作者源不可包含人工通过；验收真源必须是独立报告。"""
        document = make_manifest()
        document["features"][0]["review_status"] = "passed"
        self.assert_issue(document, "invalid_value")

    def test_parameter_type_and_range(self):
        """类型化数值支持合法零值，拒绝逆序范围及文本脚本。"""
        document = make_manifest()
        parameter = {"parameter_id": "Current", "type": "number", "unit": "points",
                     "minimum": 0, "maximum": 100, "default_value": 0,
                     "default_source": "开发测试夹具", "writable": True, "apply_when": "execute"}
        document["scenarios"][0]["parameters"] = [parameter]
        self.assertEqual([], self.validate(document))
        parameter["default_value"] = "exec unsafe"
        self.assert_issue(document, "invalid_value")
        parameter.update(default_value=0, minimum=2, maximum=1)
        self.assert_issue(document, "invalid_value")

    def test_invalid_legacy_package_path(self):
        """旧/Content前缀不是UE Game挂载点，不可作为新的目录引用。"""
        document = make_manifest()
        document["scenarios"][0]["map_asset"] = "/Content/Development/Invalid"
        self.assert_issue(document, "invalid_asset_path")

    def test_document_exports_keep_gaps_visible(self):
        """导出不能隐藏未审计插件，不能把文件存在当作资源加载成功。"""
        document = make_manifest()
        self.validate(document)
        self.assertTrue(hasattr(self.validator, "render_coverage"), "缺少覆盖表导出")
        self.assertTrue(hasattr(self.validator, "render_asset_reuse"), "缺少资源候选导出")
        coverage = self.validator.render_coverage(document)
        for plugin in document["plugins"]:
            self.assertIn(plugin["plugin_id"], coverage)
        self.assertIn("分母未知", coverage)
        self.assertIn("GamePlatformUI.ResourceBar", coverage)
        self.assertIn("不代表UE加载", self.validator.render_asset_reuse(document))

    def test_integer_default_is_exact_across_json_and_ue(self):
        """JSON数字与UE范围比较采用可精确表示区间，不能把两个整数合成同一值。"""
        document = make_manifest()
        parameter = {"parameter_id": "Count", "type": "integer", "unit": "个",
                     "minimum": 0, "maximum": 9007199254740992, "default_value": 9007199254740992,
                     "default_source": "开发夹具", "writable": True, "apply_when": "execute"}
        document["scenarios"][0]["parameters"] = [parameter]
        self.assert_issue(document, "invalid_value")

    def test_primary_feature_cannot_be_repeated_as_related(self):
        """同一功能不能在主/关联列表同时出现而隐瞒独立审核缺口。"""
        document = make_manifest()
        document["scenarios"][0]["related_feature_ids"] = [document["features"][0]["feature_id"]]
        self.assert_issue(document, "duplicate_id")

    def test_identity_reference_arrays_are_case_insensitive(self):
        """FName引用集合必须按引擎语义去重，普通人工步骤文字不套用该规则。"""
        for collection, field in (("features", "scenario_ids"), ("scenarios", "required_plugins"),
                                  ("scenarios", "review_case_ids"), ("legacy_map_routes", "scenario_ids")):
            with self.subTest(collection=collection, field=field):
                document = make_manifest()
                document["legacy_map_routes"] = [{"map_asset": document["scenarios"][0]["map_asset"],
                                                 "scenario_ids": [document["scenarios"][0]["scenario_id"]],
                                                 "requires_selection": True}]
                values = document[collection][0][field]
                values.append(values[0].lower())
                self.assert_issue(document, "duplicate_value")
        document = make_manifest()
        document["review_cases"][0]["steps"] = ["检查UI", "检查ui"]
        self.assertEqual([], self.validate(document))

    def test_reserved_none_identity_is_rejected(self):
        """引擎空FName不是可用身份；适配器、参数名及参数标识值均须拒绝大小写变体。"""
        for value in ("None", "NONE", "none", "nOnE"):
            for field in ("adapter_id", "parameter_id", "default_value"):
                with self.subTest(value=value, field=field):
                    document = make_manifest()
                    scenario = document["scenarios"][0]
                    parameter = {"parameter_id": "Choice", "type": "identifier", "unit": "无单位",
                                 "default_value": "ValidChoice", "default_source": "开发测试夹具",
                                 "writable": True, "apply_when": "execute"}
                    scenario["parameters"] = [parameter]
                    (scenario if field == "adapter_id" else parameter)[field] = value
                    self.assert_issue(document, "invalid_value")

    def test_review_version_fits_ue_int32(self):
        """作者版本必须可无损写入UE int32，最大合法值通过、溢出明确拒绝。"""
        document = make_manifest()
        document["review_cases"][0]["version"] = 2147483647
        self.assertEqual([], self.validate(document))
        document["review_cases"][0]["version"] = 2147483648
        self.assert_issue(document, "invalid_value")

    def test_noncanonical_repository_path_is_rejected(self):
        """作者路径与UE定义统一拒绝空段、当前目录段和尾斜杠，不默默归一化。"""
        for value in ("Game//Plugins", "Game/./Plugins", "Game/Plugins/"):
            with self.subTest(value=value):
                document = make_manifest()
                document["features"][0]["owner_path"] = value
                self.assert_issue(document, "unsafe_path")


if __name__ == "__main__":
    unittest.main(verbosity=2)
