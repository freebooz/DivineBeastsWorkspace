"""SSLM插件演示编目只读门禁（项目开发工具层，Python标准库）。

调用方：编目作者、离线测试及后续UE目录生成器。字段约束消费同一Schema，
关系检查读取真实插件/内容登记；不执行用户文本、不加载UE、不写作者源或验收结果。
返回结构错误与关系错误，退出0仅表示静态编目有效，绝不表示资产/功能运行成功。
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path, PurePosixPath
import re


SCHEMA_PATH = Path(__file__).resolve().parents[2] / "Docs/Implementation/PluginDemos/PluginDemoManifest.schema.json"


def _issue(issues: list, code: str, location: str, message: str) -> None:
    """追加稳定错误码与中文定位，不修改被检查的数据。"""
    issues.append({"code": code, "location": location, "message": message})


def _matches_type(value: object, name: str) -> bool:
    """严格区分布尔与整数；JSON没有NaN/Infinity，数值必须有限。"""
    checks = {"object": lambda: isinstance(value, dict), "array": lambda: isinstance(value, list),
              "string": lambda: isinstance(value, str), "boolean": lambda: type(value) is bool,
              "integer": lambda: type(value) is int,
              "number": lambda: type(value) in (int, float) and math.isfinite(value)}
    try:
        return name in checks and checks[name]()
    except OverflowError:
        # UE数值字段最终需要有限浮点表示；拒绝超大整数而不是让门禁进程崩溃。
        return False


def _validate_shape(value: object, rule: dict, schema: dict, location: str, issues: list) -> None:
    """执行本合同使用的JSON Schema标准关键字子集；引用仅限本文件$defs。

    无外部引用或网络解析，避免字段合同读取变成任意文件/URL访问。
    非法结构先定位再停止本分支，后续关系检查仅在结构完整时执行。
    """
    if "$ref" in rule:
        rule = schema["$defs"][rule["$ref"].removeprefix("#/$defs/")]
    expected = rule.get("type", [])
    expected = [expected] if isinstance(expected, str) else expected
    if expected and not any(_matches_type(value, name) for name in expected):
        code = "invalid_value" if type(value) in (int, float) and "number" in expected else "invalid_type"
        # 布尔误作超时也应是非法值，不允许Python的bool继承int绕过合同。
        if type(value) is bool and "number" in expected:
            code = "invalid_value"
        _issue(issues, code, location, "字段类型不符或数值不是有限数。")
        return
    if "const" in rule and value != rule["const"]:
        _issue(issues, "invalid_value", location, "字段版本或固定值不匹配。")
    if "enum" in rule and value not in rule["enum"]:
        _issue(issues, "invalid_value", location, "字段不属于允许的枚举值。")
    if isinstance(value, dict):
        for key in rule.get("required", []):
            if key not in value:
                _issue(issues, "required_value", location + "." + key, "缺少必需字段。")
        properties = rule.get("properties", {})
        for key, item in value.items():
            if key in properties:
                _validate_shape(item, properties[key], schema, location + "." + key, issues)
            elif rule.get("additionalProperties") is False:
                _issue(issues, "unknown_field", location + "." + key, "未声明字段；禁止嵌入签收或额外执行命令。")
            elif isinstance(rule.get("additionalProperties"), dict):
                _validate_shape(item, rule["additionalProperties"], schema, location + "." + key, issues)
    elif isinstance(value, list):
        if len(value) < rule.get("minItems", 0):
            _issue(issues, "required_value", location, "列表缺少必需内容。")
        if rule.get("uniqueItems"):
            # 列表元素可能是对象，因此不能直接转set；规范化JSON只用于重复检测。
            identities = [json.dumps(item, sort_keys=True, ensure_ascii=False) for item in value]
            if len(set(identities)) != len(identities):
                _issue(issues, "duplicate_value", location, "列表包含重复项。")
            elif rule.get("items", {}).get("$ref") == "#/$defs/identity" and all(isinstance(item, str) for item in value):
                # JSON字符串本身区分大小写；只有映射到FName的身份集合增加引擎语义约束。
                if len({item.casefold() for item in value}) != len(value):
                    _issue(issues, "duplicate_value", location, "身份列表按UE FName语义包含大小写重复项。")
        for index, item in enumerate(value):
            _validate_shape(item, rule.get("items", {}), schema, f"{location}[{index}]", issues)
    elif isinstance(value, str):
        if len(value) > rule.get("maxLength", len(value)):
            _issue(issues, "invalid_value", location, "字符串超出引擎身份长度边界。")
        if len(value.strip()) < rule.get("minLength", 0):
            _issue(issues, "required_value", location, "内容不能为空白。")
        if "pattern" in rule and not re.fullmatch(rule["pattern"], value):
            _issue(issues, "invalid_value", location, "身份或路径格式不符合合同。")
    elif type(value) in (int, float):
        if "minimum" in rule and value < rule["minimum"]:
            _issue(issues, "invalid_value", location, "数值小于允许下界。")
        if "maximum" in rule and value > rule["maximum"]:
            _issue(issues, "invalid_value", location, "数值大于允许上界。")
        if "exclusiveMinimum" in rule and value <= rule["exclusiveMinimum"]:
            _issue(issues, "invalid_value", location, "数值必须严格大于允许下界。")


def _repository_path(root: Path, value: str, location: str, issues: list) -> Path | None:
    """解析仓库内相对路径并拒绝越界/符号链接逃逸；不读取外部内容。"""
    parts = PurePosixPath(value)
    # PurePosixPath会消除空段和当前目录段，必须在归一化之前拒绝，保持与UE合同一致。
    if parts.is_absolute() or any(part in ("", ".", "..") for part in value.split("/")) or "\\" in value or ":" in value:
        _issue(issues, "unsafe_path", location, "路径必须是仓库内使用正斜杠的相对路径。")
        return None
    try:
        path = (root / value).resolve()
        if not path.is_relative_to(root):
            _issue(issues, "unsafe_path", location, "解析后的路径越出工作空间。")
            return None
        if not path.exists():
            _issue(issues, "path_missing", location, "文件或责任目录不存在：" + value)
            return None
        return path
    except (OSError, ValueError):
        _issue(issues, "unsafe_path", location, "无法安全解析路径。")
        return None


def _index(records: list, key: str, location: str, issues: list) -> dict:
    """按FName兼容的大小写折叠检查身份；重复时保留错误而非任取首项运行。"""
    result = {}
    for record in records:
        identity = record[key].casefold()
        if identity in result:
            _issue(issues, "duplicate_id", location, "重复身份：" + record[key])
        else:
            result[identity] = record
    return result


def _reference(identity: str, index: dict, location: str, issues: list) -> dict | None:
    """查询已登记身份，缺失时保留可展示诊断，不补造默认对象。"""
    record = index.get(identity.casefold())
    if record is None:
        _issue(issues, "reference_missing", location, "未登记引用：" + identity)
    return record


def _check_plugins(records: list, root: Path, issues: list) -> None:
    """交叉核对描述、模块类别、三层归属与内容注册；不硬编码47/18上限。"""
    actual = {}
    for folder, layer in (("GamePlatform", "Platform"), ("MobaCommon", "Moba"), ("DivineBeasts", "Project"), ("ThirdParty", "Project")):
        for path in sorted((root / "Game/Plugins" / folder).rglob("*.uplugin")):
            relative = path.relative_to(root).as_posix()
            safe = _repository_path(root, relative, relative, issues)
            if safe is None:
                continue
            try:
                descriptor = json.loads(safe.read_text(encoding="utf-8-sig"))
                modules = descriptor.get("Modules", [])
                if not isinstance(modules, list):
                    raise ValueError("Modules必须是数组")
                item = (path.stem, relative, layer, "code" if modules else "content_pack")
                if path.stem.casefold() in actual:
                    _issue(issues, "duplicate_id", relative, "实际描述文件存在重复插件身份。")
                actual[path.stem.casefold()] = item
            except (OSError, ValueError, AttributeError) as error:
                _issue(issues, "descriptor_invalid", relative, "插件描述不可读：" + str(error))
    registered = _index(records, "plugin_id", "plugins", issues)
    for identity, item in actual.items():
        record = registered.get(identity)
        if record is None:
            _issue(issues, "plugin_missing", "plugins", "真实插件未登记：" + item[0])
        elif (record["plugin_id"], record["descriptor_path"], record["architecture_layer"], record["plugin_kind"]) != item:
            _issue(issues, "plugin_mismatch", "plugins." + item[0], "插件身份/路径/层级/类别与真实描述不一致。")
    for identity, record in registered.items():
        if identity not in actual:
            _issue(issues, "plugin_unknown", "plugins", "登记插件无真实描述：" + record["plugin_id"])
    registry_path = root / "Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json"
    try:
        registry = json.loads(registry_path.read_text(encoding="utf-8-sig"))
        entries = registry["ContentPacks"]
        expected = {item[0]: item[1] for item in actual.values() if item[3] == "content_pack"}
        seen = set()
        for entry in entries:
            name = entry["Name"]
            relative = "Game/Plugins/DivineBeasts/ContentPacks/" + entry["RelativePath"] + "/" + name + ".uplugin"
            if name in seen or expected.get(name) != relative:
                _issue(issues, "content_registry_mismatch", "plugins", "内容登记重复或位置不匹配：" + name)
            seen.add(name)
        if seen != set(expected):
            _issue(issues, "content_registry_mismatch", "plugins", "内容包身份集合与真实描述不一致。")
    except (OSError, ValueError, KeyError, TypeError) as error:
        _issue(issues, "content_registry_invalid", "plugins", "内容登记不可读：" + str(error))


def validate_manifest(document: dict, workspace: Path) -> list[dict]:
    """返回全部可安全发现的问题；空列表仅代表静态合同通过，不改输入状态。"""
    issues = []
    root = workspace.resolve()
    try:
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return [{"code": "schema_unavailable", "location": "$", "message": "合同不可读：" + str(error)}]
    _validate_shape(document, schema, schema, "$", issues)
    if issues:
        return issues
    _check_plugins(document["plugins"], root, issues)
    if document.get("manual_page_contract_version") == 1:
        for plugin in document["plugins"]:
            expected = "/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_" + plugin["plugin_id"]
            if plugin.get("test_page_asset") != expected:
                _issue(issues, "manual_page.identity", "plugins." + plugin["plugin_id"], "独立页路径必须精确绑定本插件，禁止跨插件别名、越界路径或缺失页面声明。")
            origin = "third_party" if plugin["descriptor_path"].startswith("Game/Plugins/ThirdParty/") else "first_party"
            if plugin.get("source_kind") != origin:
                _issue(issues, "manual_page.source", "plugins." + plugin["plugin_id"], "插件来源类型与真实描述路径不符。")
    plugins = _index(document["plugins"], "plugin_id", "plugins", [])
    features = _index(document["features"], "feature_id", "features", issues)
    scenarios = _index(document["scenarios"], "scenario_id", "scenarios", issues)
    reviews = _index(document["review_cases"], "review_case_id", "review_cases", issues)
    for identity, feature in features.items():
        _reference(feature["plugin_id"], plugins, identity, issues)
        _repository_path(root, feature["owner_path"], identity, issues)
        for evidence in feature["implementation_evidence"]:
            evidence_path = _repository_path(root, evidence["path"], identity, issues)
            if evidence_path and not evidence_path.is_file():
                _issue(issues, "evidence_not_file", identity, "实现依据必须定位到真实文件。")
        if feature["implementation_status"] == "implemented" and not feature["implementation_evidence"]:
            _issue(issues, "required_value", identity, "已实现功能必须有真实源码/合同依据。")
        if feature["integration_status"] in ("wired", "launchable") and not feature["scenario_ids"]:
            _issue(issues, "required_value", identity, "已接线功能必须有案例。")
        for scenario_id in feature["scenario_ids"]:
            scenario = _reference(scenario_id, scenarios, identity, issues)
            if scenario and identity not in [scenario["feature_id"].casefold()] + [key.casefold() for key in scenario["related_feature_ids"]]:
                _issue(issues, "ownership_mismatch", identity, "案例未声明该主功能或关联功能。")
    for identity, scenario in scenarios.items():
        involved = [scenario["feature_id"]] + scenario["related_feature_ids"]
        if len({key.casefold() for key in involved}) != len(involved):
            _issue(issues, "duplicate_id", identity, "主/关联功能含重复身份。")
        for feature_id in involved:
            feature = _reference(feature_id, features, identity, issues)
            if feature and identity not in [key.casefold() for key in feature["scenario_ids"]]:
                _issue(issues, "ownership_mismatch", identity, "功能未反向登记该案例。")
            if feature and feature["plugin_id"].casefold() not in [key.casefold() for key in scenario["required_plugins"]]:
                _issue(issues, "owner_dependency_missing", identity, "案例必须依赖每个被核验功能的所有者插件。")
        for plugin in scenario["required_plugins"]:
            _reference(plugin, plugins, identity, issues)
        if scenario["run_mode"] == "editor" and scenario["targets"] != ["Editor"]:
            _issue(issues, "invalid_value", identity, "编辑器制作只能声明Editor目标。")
        labels = scenario.get('operation_labels', {})
        folded_labels = set()
        declared_operations = {operation.casefold() for operation in scenario['operations']}
        for operation, label in labels.items():
            _validate_shape(operation, {'$ref': '#/$defs/identity'}, schema, identity + '.operation_labels', issues)
            if operation.casefold() in folded_labels:
                _issue(issues, 'duplicate_id', identity, '操作标签含FName大小写重复身份。')
            folded_labels.add(operation.casefold())
            if operation.casefold() not in declared_operations:
                _issue(issues, 'operation_label_unknown', identity, '标签不能登记案例未声明的操作。')
            if not label.strip():
                _issue(issues, 'invalid_value', identity, '具名操作标签不能只有空白。')
        for operation in scenario['operations']:
            if operation.casefold() not in {'execute', 'pause', 'resume', 'stop', 'reset'} and operation.casefold() not in folded_labels:
                _issue(issues, 'operation_label_missing', identity, '具名操作需要可读中文标签，执行仍受适配白名单约束。')
        for asset in [scenario["map_asset"], scenario["widget_asset"]] + scenario["definition_assets"]:
            if asset and (not re.fullmatch(r"/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+(?:\.[A-Za-z0-9_]+)?", asset) or asset.startswith("/Content/")):
                _issue(issues, "invalid_asset_path", identity, "不是合法挂载软引用：" + asset)
        parameters = _index(scenario["parameters"], "parameter_id", identity, issues)
        for name, parameter in parameters.items():
            kind = parameter["type"]
            default = parameter["default_value"]
            if not _matches_type(default, "string" if kind in ("identifier", "asset_path") else kind):
                _issue(issues, "invalid_value", name, "参数默认值与声明类型不符。")
            elif kind in ("integer", "number"):
                low, high = parameter.get("minimum"), parameter.get("maximum")
                if low is None or high is None or not low <= default <= high:
                    _issue(issues, "invalid_value", name, "数值参数必须声明有序范围且默认值位于范围内。")
                if kind == "integer" and abs(default) > 9007199254740991:
                    _issue(issues, "invalid_value", name, "整数参数超出JSON/UE范围比较可精确表达的区间。")
            elif kind == "identifier":
                # 参数值与记录身份共享一个Schema规则，避免允许脚本或UE保留空名。
                _validate_shape(default, {"$ref": "#/$defs/identity"}, schema, name + ".default_value", issues)
            elif kind == "asset_path":
                if (len(default) >= 1024 or
                        not re.fullmatch(r"/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+\.[A-Za-z0-9_]+", default) or
                        default.startswith('/Content/')):
                    _issue(issues, "invalid_asset_path", name, "资源参数必须是明确的挂载顶层对象路径，不允许磁盘/子对象/脚本文本。")
        covered = set()
        for review_id in scenario["review_case_ids"]:
            review = _reference(review_id, reviews, identity, issues)
            if review:
                covered.add(review["feature_id"].casefold())
                if review["scenario_id"].casefold() != identity:
                    _issue(issues, "ownership_mismatch", identity, "检查用例属于其他案例。")
        if covered != {key.casefold() for key in involved}:
            _issue(issues, "review_coverage_missing", identity, "主功能及关联功能需要各自独立检查用例。")
    for identity, review in reviews.items():
        scenario = _reference(review["scenario_id"], scenarios, identity, issues)
        _reference(review["feature_id"], features, identity, issues)
        if scenario and identity not in [key.casefold() for key in scenario["review_case_ids"]]:
            _issue(issues, "ownership_mismatch", identity, "用例未被案例反向登记。")
    seen_maps = set()
    for route in document["legacy_map_routes"]:
        location = route["map_asset"]
        if location.casefold() in seen_maps:
            _issue(issues, "duplicate_id", location, "旧地图兼容路由重复。")
        seen_maps.add(location.casefold())
        if len(route["scenario_ids"]) > 1 and not route["requires_selection"]:
            _issue(issues, "legacy_route_ambiguous", location, "一对多映射必须请求用户选择。")
        for scenario_id in route["scenario_ids"]:
            scenario = _reference(scenario_id, scenarios, location, issues)
            if scenario and scenario["map_asset"].casefold() != location.casefold():
                _issue(issues, "ownership_mismatch", location, "兼容路由与案例地图不匹配。")
    for record in document.get("discovered_maps", []):
        path = _repository_path(root, record["repository_path"], "discovered_maps", issues)
        if path and (not record["repository_path"].startswith("Game/Content/Development/") or
                     path.suffix != ".umap" or record["asset_package"] != "/Game/" + record["repository_path"][len("Game/Content/"):-5]):
            _issue(issues, "invalid_asset_path", "discovered_maps", "地图文件与/Game包身份不一致。")
    return issues


def summarize_coverage(document: dict) -> dict:
    """汇总登记事实；未完成全部功能审计时分母未知，百分比必须为null。

    作者源不持有签收结果，所以当前版本人工通过数不可从其推断，固定为0项可证实记录。
    后续运行端从独立报告读取实跑/人工统计，不覆盖本作者源。
    """
    features = document["features"]
    unknown = sum(plugin["feature_audit_status"] != "complete" for plugin in document["plugins"])
    wired = sum(feature["integration_status"] in ("wired", "launchable") for feature in features)
    return {"plugins": len(document["plugins"]), "unreviewed_plugins": unknown, "known_features": len(features),
            "wired_features": wired, "human_approved_features": 0,
            "coverage_percent": None if unknown or not features else round(100 * wired / len(features), 2)}


def _markdown_cell(value: str) -> str:
    """保持作者文字为单个表格单元，避免竖线/换行改变导出结构。"""
    return value.replace("|", "\\|").replace("\r", " ").replace("\n", " ")


def render_coverage(document: dict) -> str:
    """从已验证作者源生成中文覆盖表；不把历史能力线索当成实现依据。"""
    summary = summarize_coverage(document)
    lines = ["# SSLM插件演示功能覆盖表（Schema2派生）", "",
             "来源：PluginDemoManifest.json；由ValidatePluginDemoManifest.py导出，不独立维护状态。",
             "历史线索保留于History/PluginDemoManifest.v1.json，不能视为实际功能验收。", "",
             f"登记插件 {summary['plugins']}；范围未完整审计 {summary['unreviewed_plugins']}；已登记功能 {summary['known_features']}；已接线 {summary['wired_features']}。",
             "覆盖率：" + ("分母未知，不显示百分比。" if summary["coverage_percent"] is None else str(summary["coverage_percent"]) + "%（仅接入比例）。"),
             "当前作者源不携带运行/人工签收，真实结果必须查独立版本化报告。", "",
             "| 插件 | 三层归属 | 类别 | 功能审计 | 已登记功能数 |", "| --- | --- | --- | --- | ---: |"]
    layer_names = {"Platform": "平台层", "Moba": "MOBA通用层", "Project": "项目层"}
    audit_names = {"unreviewed": "待审计", "partial": "部分审计", "complete": "范围审计完成"}
    for plugin in sorted(document["plugins"], key=lambda item: item["plugin_id"]):
        count = sum(feature["plugin_id"].casefold() == plugin["plugin_id"].casefold() for feature in document["features"])
        kind = "代码" if plugin["plugin_kind"] == "code" else "内容包"
        lines.append(f"| {plugin['plugin_id']} | {layer_names[plugin['architecture_layer']]} | {kind} | {audit_names[plugin['feature_audit_status']]} | {count} |")
    lines += ["", "## 已登记独立功能", "", "| 身份 | 中文目的 | 实现 | 接入 | 案例数 |", "| --- | --- | --- | --- | ---: |"]
    for feature in document["features"]:
        lines.append(f"| {feature['feature_id']} | {_markdown_cell(feature['purpose'])} | {feature['implementation_status']} | {feature['integration_status']} | {len(feature['scenario_ids'])} |")
    if not document["features"]:
        lines += ["", "尚未完成独立功能审计；不把Public API数量或地图数量换算成功能完成数。"]
    return "\n".join(lines) + "\n"


def render_asset_reuse(document: dict) -> str:
    """导出文件级地图候选；不猜所有者，不代表UE加载、交互或人工通过。"""
    lines = ["# SSLM开发地图复用候选（Schema2派生）", "",
             "来源：PluginDemoManifest.json；只证实文件路径，不代表UE加载、交互或人工通过。",
             "历史/Content前缀已按Game/Content真实文件改为/Game；功能所有权留待逐场景审计。", "",
             "| 地图包路径 | 分类 | 文件验证 | 运行/人工 |", "| --- | --- | --- | --- |"]
    for record in document.get("discovered_maps", []):
        lines.append(f"| {record['asset_package']} | {record['kind']} | file_only | 未运行/未核验 |")
    return "\n".join(lines) + "\n"


def main() -> int:
    """命令行只读入口：0静态通过，1合同失败，2输入不可读取；输出中文JSON诊断。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--print-document", choices=("coverage", "assets"), help="打印派生Markdown；不写文件")
    parser.add_argument("--check-docs", action="store_true", help="只读比较同目录覆盖表/地图表是否与源一致")
    parser.add_argument("--print-cook-maps", action="store_true", help="打印已验证案例的精确开发地图白名单；不执行Cook")
    args = parser.parse_args()
    try:
        document = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        print(json.dumps({"error": "输入不可读", "detail": str(error)}, ensure_ascii=False))
        return 2
    issues = validate_manifest(document, args.workspace)
    if not issues and args.print_cook_maps:
        maps = sorted({item["map_asset"].split(".", 1)[0] for item in document["scenarios"] if item["map_asset"]})
        if not maps:
            _issue(issues, "no_cook_maps", "scenarios", "未登记真实案例地图，不能扩大Cook集合。")
        else:
            print(json.dumps(maps, ensure_ascii=False))
            return 0
    if not issues and args.check_docs:
        for name, rendered in (("FeatureCoverage.md", render_coverage(document)), ("AssetReuseMap.md", render_asset_reuse(document))):
            try:
                if args.manifest.with_name(name).read_text(encoding="utf-8-sig") != rendered:
                    _issue(issues, "derived_document_stale", name, "派生文档与作者源不一致，请重新导出。")
            except OSError:
                _issue(issues, "derived_document_missing", name, "派生文档不存在或不可读。")
    if not issues and args.print_document:
        print(render_coverage(document) if args.print_document == "coverage" else render_asset_reuse(document), end="")
        return 0
    report = {"scope": "静态编目，不代表UE运行/人工验收", "issues": issues}
    if not issues:
        report["coverage"] = summarize_coverage(document)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
