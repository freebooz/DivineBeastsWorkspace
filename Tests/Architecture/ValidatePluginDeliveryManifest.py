#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读核对插件交付台账与真实描述、模块、内容登记及文档入口；不执行UE或宣称资产验收。
Passed只检查RunId格式及证据路径存在，不解析证据内容、不证明运行成功或证据归属于该RunId。
用法：python Tests/Architecture/ValidatePluginDeliveryManifest.py；0=一致，1=台账缺漏/漂移。
"""
from __future__ import annotations
import json
import sys
from pathlib import Path, PurePosixPath, PureWindowsPath
from uuid import UUID

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "Docs/Implementation/PluginArchitectureOptimization/PluginDeliveryManifest.json"


def validate(root: Path, manifest: dict) -> list[str]:
    """只读核对给定工作区；返回全部合同错误，不把登记状态升级为运行证明。"""
    root = root.resolve()
    errors: list[str] = []

    def file_at(value: object, label: str, *, allow_directory: bool = False) -> Path | None:
        """描述/入口必须为文件，证据也可指向已有目录；规范路径与解析后根边界同时校验。"""
        if not isinstance(value, str) or not value or "\\" in value:
            errors.append(f"{label}必须是正斜杠项目相对路径")
            return None
        relative = PurePosixPath(value)
        # 不先折叠..或重复分隔符；也拒绝Windows盘符相对路径，避免不同宿主产生不同解释。
        if (not relative.parts or relative.is_absolute() or PureWindowsPath(value).drive
                or ".." in relative.parts or relative.as_posix() != value):
            errors.append(f"{label}必须是规范项目相对路径，禁止绝对路径、盘符、.或..分量: {value}")
            return None
        candidate = (root / value).resolve()
        if (not candidate.is_relative_to(root)
                or not (candidate.is_file() or (allow_directory and candidate.is_dir()))):
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
        if state == "Passed":
            run_id = scenario.get("RunId")
            try:
                if not isinstance(run_id, str):
                    raise ValueError("RunId不是字符串")
                UUID(run_id)
            except ValueError:
                errors.append(f"场景{sid} Passed的RunId必须是合法GUID字符串")
            evidence = scenario.get("Evidence")
            if not isinstance(evidence, list) or not evidence:
                errors.append(f"场景{sid} Passed的Evidence必须是非空字符串列表")
            else:
                # 此处只证明每个路径在工作区内存在；运行状态和RunId关联由执行入口/人工复核负责。
                for index, value in enumerate(evidence):
                    file_at(value, f"场景{sid} Evidence[{index}]", allow_directory=True)
        if state == "Blocked" and not scenario.get("BlockerZh"):
            errors.append(f"场景{sid}未说明阻塞")
    # 先保留全部物理描述再建身份索引；Windows不区分大小写，不能让字典覆盖隐藏副本。
    descriptor_groups: dict[str, list[Path]] = {}
    for path in (root / "Game/Plugins").rglob("*.uplugin"):
        descriptor_groups.setdefault(path.stem.casefold(), []).append(path)
    actual = {}
    for paths in descriptor_groups.values():
        if len(paths) != 1:
            errors.append("物理插件描述重名（忽略大小写）: " + ", ".join(
                sorted(path.relative_to(root).as_posix() for path in paths)))
            continue
        actual[paths[0].stem] = paths[0]
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
    print(f"PLUGIN_DELIVERY_MANIFEST_PASS: plugins={len(plugins)} code={sum(p['Kind']=='Code' for p in plugins)} content={sum(p['Kind']=='Content' for p in plugins)} reserved_modules={reserved}; 文档/登记及证据存在性一致，不证明运行成功、RunId关联或UE/生产验收")
    return 0

if __name__ == "__main__":
    sys.exit(main())
