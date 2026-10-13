"""SSLM开发编目DataAsset作者：项目工具层，只在锁定正式UE5.8中写派生目录。

JSON为唯一人工真源；调用方为开发资产制作流程。先完整静态校验再接触UE，
不制作Widget/地图，不启动编译/网络，不改逻辑身份，不签人工结论。
重复运行更新同一资产，失败不保存半成品；生成后必须另行重启回读与运行验证。
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
PACKAGE = "/DBAUIPack_Core/Development/PluginDemos/Definitions/DA_DBA_PluginDemoCatalog"


def read_author_source(manifest_path: str) -> tuple[dict, str]:
    """只读校验全部关系，返回文档及原始字节SHA256；不调用UE或写证据。"""
    path = Path(manifest_path).resolve()
    if not path.is_relative_to(WORKSPACE):
        raise ValueError("作者源必须在当前正式工作空间。")
    raw = path.read_bytes()
    document = json.loads(raw.decode("utf-8-sig"))
    validator_path = WORKSPACE / "Tests/Architecture/ValidatePluginDemoManifest.py"
    spec = importlib.util.spec_from_file_location("plugin_demo_validator", validator_path)
    validator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(validator)
    issues = validator.validate_manifest(document, WORKSPACE)
    if issues:
        raise ValueError(json.dumps(issues, ensure_ascii=False))
    return document, hashlib.sha256(raw).hexdigest()


def build_catalog(manifest_path: str) -> dict:
    """通过真实引擎反射创建/更新目录；类未加载或工程不匹配明确拒绝，不生成文本占位资产。"""
    document, fingerprint = read_author_source(manifest_path)
    import unreal
    actual_project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if actual_project != (WORKSPACE / "Game/DivineBeastsArena.uproject").resolve():
        raise RuntimeError("拒绝修改其他编辑器工程。")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("需要锁定UE5.8。")
    definition_class = getattr(unreal, "DivineBeastsDemoCatalogDefinition", None)
    if definition_class is None:
        raise RuntimeError("BlockedByEditorBinary：新目录反射类未加载；本脚本不会触发编译。")

    def value(kind: str, **fields):
        """通过USTRUCT（原生反射结构）构造器一次传入已审查字段。

        UE5.8部分字段的EditDefaultsOnly（仅默认值可编辑）会拒绝对临时结构实例
        逐个set_editor_property，导致真实作者中途失败；构造器仍由反射严格检查
        未知字段、类型和枚举。禁止catch后跳过字段或以默认值冒充真实目录。
        """
        struct_class = getattr(unreal, kind, None)
        if struct_class is None:
            raise RuntimeError("缺少真实编辑器反射类型：" + kind)
        return struct_class(**fields)

    def parameter(record: dict):
        """封闭四类型一对一转换；不用浮点保存整数或任意脚本字符串。"""
        payload = {"boolean": "b_boolean_value", "integer": "integer_value", "number": "number_value", "identifier": "identifier_value"}
        # UE Python的b前缀属性通常去除b；用反射实测名称，不臆测失败后默认数值。
        payload["boolean"] = "boolean_value"
        parameter_value = value("DivineBeastsDemoParameterValue", type=getattr(unreal.DivineBeastsDemoParameterType, record["type"].upper()), **{payload[record["type"]]: record["default_value"]})
        return value("DivineBeastsDemoParameterDefinition", parameter_id=record["parameter_id"], unit=record["unit"],
                     default_value=parameter_value, minimum=record.get("minimum", 0), maximum=record.get("maximum", 0),
                     default_source=record["default_source"], writable=record["writable"], apply_when=record["apply_when"])

    fields = {
        "source_fingerprint": fingerprint,
        "logical_id": value("GamePlatformId", **{"namespace": "divinebeasts.demo", "name": "catalog", "logical_version": 1}),
        "data_version": value("GamePlatformDataVersion", schema_version=2, content_revision=1),
        "plugins": [value("DivineBeastsDemoPluginEntry", **p) for p in document["plugins"]],
        "features": [], "scenarios": [], "review_cases": [], "legacy_map_routes": []}
    for feature in document["features"]:
        fields["features"].append(value("DivineBeastsDemoFeatureEntry", **{k: feature[k] for k in
            ("feature_id", "plugin_id", "title", "purpose", "implementation_status", "integration_status", "owner_path", "scenario_ids")},
            implementation_evidence=[value("DivineBeastsDemoImplementationEvidence", **e) for e in feature["implementation_evidence"]]))
    for record in document["scenarios"]:
        item = {k: record[k] for k in ("scenario_id", "feature_id", "related_feature_ids", "title", "adapter_id", "run_mode", "targets", "required_plugins", "preconditions", "operations", "prepare_timeout_seconds", "stop_timeout_seconds", "review_case_ids")}
        item.update(template_id=record["template"], parameters=[parameter(p) for p in record["parameters"]],
                    map_asset=unreal.SoftObjectPath(record["map_asset"]), widget_asset=unreal.SoftObjectPath(record["widget_asset"]),
                    definition_assets=[unreal.SoftObjectPath(p) for p in record["definition_assets"]])
        fields["scenarios"].append(value("DivineBeastsDemoScenarioDefinition", **item))
    for record in document["review_cases"]:
        fields["review_cases"].append(value("DivineBeastsDemoReviewCaseDefinition", **record))
    for record in document["legacy_map_routes"]:
        fields["legacy_map_routes"].append(value("DivineBeastsDemoLegacyMapRoute", map_asset=unreal.SoftObjectPath(record["map_asset"]),
            scenario_ids=record["scenario_ids"], requires_selection=record["requires_selection"]))
    # 全部值先构造成功；已有其他父类资产不覆盖，不创建第二份同名目录。
    assets = unreal.EditorAssetLibrary
    asset = assets.load_asset(PACKAGE) if assets.does_asset_exist(PACKAGE) else None
    if asset is not None and not isinstance(asset, definition_class):
        raise RuntimeError("既有目录父类不匹配，需独立迁移，不覆盖。")
    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", definition_class)
        directory, _, name = PACKAGE.rpartition("/")
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, definition_class, factory)
    if asset is None:
        raise RuntimeError("真实DataAsset创建失败。")
    previous = {key: asset.get_editor_property(key) for key in fields}
    try:
        for key, item in fields.items():
            asset.set_editor_property(key, item)
        if asset.get_editor_property("source_fingerprint") != fingerprint or not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("真实目录保存或指纹回读失败。")
    except Exception:
        for key, item in previous.items():
            asset.set_editor_property(key, item)
        raise
    return {"package": PACKAGE, "source_fingerprint": fingerprint, "saved": True,
            "independent_reload": "not_run", "runtime": "not_run", "human_review": "deferred"}


if __name__ == "__main__":
    print(json.dumps(build_catalog(str(WORKSPACE / "Docs/Implementation/PluginDemos/PluginDemoManifest.json")), ensure_ascii=False))
