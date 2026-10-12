#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读核对插件交付台账与真实描述、模块、内容登记及文档入口；不执行UE或宣称资产验收。
用法：python Tests/Architecture/ValidatePluginDeliveryManifest.py；0=一致，1=台账缺漏/漂移。
"""
from __future__ import annotations
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json"


def validate(root: Path, manifest: dict) -> list[str]:
    errors: list[str] = []
    def file_at(value: object, label: str) -> Path | None:
        if not isinstance(value, str) or not value or "\\\\" in value:
            errors.append(f"{label}必须是正斜杠项目相对路径")
            return None
        candidate = (root / value).resolve()
        if Path(value).is_absolute() or not candidate.is_relative_to(root.resolve()) or not candidate.is_file():
            errors.append(f"{label}缺失或越界: {value}")
            return None
        return candidate
    if manifest.get("SchemaVersion") != 1:
        errors.append("不支持的SchemaVersion")
    file_at(manifest.get("MainProject"), "MainProject")
    registry_path = file_at(manifest.get("ContentRegistry"), "ContentRegistry")
    records = manifest.get("Plugins", [])
    scenarios = manifest.get("Scenarios", [])
    if not isinstance(records, list) or not isinstance(scenarios, list):
        return errors + ["Plugins/Scenarios必须为数组"]
    scenarios_by_id = {}
    for scenario in scenarios:
        sid = scenario.get("Id")
        if not isinstance(sid, str) or not sid or sid in scenarios_by_id:
            errors.append(f"场景ID缺失/重复: {sid}")
            continue
        scenarios_by_id[sid] = scenario
        file_at(scenario.get("Entry"), f"场景{sid}入口")
        if not scenario.get("Steps") or not scenario.get("Expected") or not scenario.get("Profiles"):
            errors.append(f"场景{sid}缺少步骤、预期或Profile")
        state = scenario.get("Status")
        if state not in ("NotRun", "Blocked", "Passed", "Failed", "HistoricalOnly"):
            errors.append(f"场景{sid}状态无效")
        if state == "Passed" and (not scenario.get("RunId") or not scenario.get("Evidence")):
            errors.append(f"场景{sid}声明通过但无本次运行/证据")
        if state == "Blocked" and not scenario.get("BlockerZh"):
            errors.append(f"场景{sid}未说明阻塞")
    actual = {p.stem: p for p in (root / "Game/Plugins").rglob("*.uplugin")}
    seen = set()
    content = set()
    for record in records:
        ident = record.get("Id")
        if ident in seen or ident not in actual:
            errors.append(f"插件ID重复或不存在: {ident}")
            continue
        seen.add(ident)
        descriptor_path = file_at(record.get("Descriptor"), f"{ident}描述")
        file_at(record.get("Readme"), f"{ident}README")
        if descriptor_path != actual[ident].resolve():
            errors.append(f"{ident}描述身份/路径错配")
            continue
        descriptor = json.loads(descriptor_path.read_text(encoding="utf-8-sig"))
        actual_modules = {m["Name"]: m["Type"] for m in descriptor.get("Modules", [])}
        registered = record.get("Modules", [])
        names = [m.get("Name") for m in registered]
        if len(names) != len(set(names)) or {m.get("Name"): m.get("HostType") for m in registered} != actual_modules:
            errors.append(f"{ident}模块登记与.uplugin不一致")
        expected_kind = "Code" if actual_modules else "Content"
        if record.get("Kind") != expected_kind:
            errors.append(f"{ident}代码/内容分类错误")
        if expected_kind == "Content":
            content.add(ident)
            if not descriptor.get("CanContainContent"):
                errors.append(f"{ident}纯内容描述未允许内容")
        for module in registered:
            if module.get("Maturity") not in ("Reserved", "SourcePresent"):
                errors.append(f"{ident}/{module.get('Name')}不得把源码存在自动标记生产通过")
        if not record.get("Scenarios") or any(s not in scenarios_by_id for s in record.get("Scenarios", [])):
            errors.append(f"{ident}验收场景缺失或引用不存在")
    if set(actual) != seen:
        errors.append("台账漏登插件: " + ", ".join(sorted(set(actual) - seen)))
    if registry_path:
        registry = json.loads(registry_path.read_text(encoding="utf-8-sig"))
        if content != {p["Name"] for p in registry.get("ContentPacks", [])}:
            errors.append("纯内容台账与ContentPackRegistry不一致")
    policy = manifest.get("ReservedModulePolicy", {})
    if policy.get("State") != "Reserved" or policy.get("ProductionCapability") is not False:
        errors.append("保留模块不能算作生产能力")
    return errors


def main() -> int:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
        errors = validate(ROOT, manifest)
    except (OSError, ValueError, TypeError, KeyError, AttributeError) as error:
        print(f"PLUGIN_DELIVERY_MANIFEST_FAIL: {error}")
        return 1
    for error in errors:
        print(f"FAIL: {error}")
    if errors:
        return 1
    plugins = manifest["Plugins"]
    reserved = sum(m["Maturity"] == "Reserved" for p in plugins for m in p["Modules"])
    print(f"PLUGIN_DELIVERY_MANIFEST_PASS: plugins={len(plugins)} code={sum(p['Kind']=='Code' for p in plugins)} content={sum(p['Kind']=='Content' for p in plugins)} reserved_modules={reserved}; 文档/登记一致，不代表UE或生产验收")
    return 0

if __name__ == "__main__":
    sys.exit(main())
