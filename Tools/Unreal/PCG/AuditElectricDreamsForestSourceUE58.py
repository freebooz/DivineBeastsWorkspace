#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Epic Electric Dreams 森林示例源资产的UE原生只读依赖审计。

归属：SSLM项目层开发工具，不属于GamePlatformPCG运行模块。
输入：在Epic/Fab授权下载的Electric Dreams源工程内加载本脚本。
输出：源工程Saved/Validation/ElectricDreamsSource下不可覆盖的JSON清单。
边界：仅读取UE AssetRegistry包及引用，不迁移/复制/修改源资产，不访问Fab令牌。
用途：为后续在隔离迁移工程中进行Migrate、Redirector修复、材质兼容审核提供依据。
注意：AssetRegistry及磁盘来源不等于用户账号授权；UE导入和SSLM场景生成另行验收。
"""
from __future__ import annotations

import argparse
from collections import Counter, deque
from datetime import datetime
import json
from pathlib import Path

FOREST_MAP = "/Game/Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest"
GRAPH_ROOT = "/Game/PCG"
MAX_PACKAGES = 15000
MAX_GRAPH_SEEDS = 24
ASSET_CLASSES = {
    "StaticMesh", "Material", "MaterialInstanceConstant", "MaterialFunction",
    "MaterialFunctionMaterialLayer", "MaterialFunctionMaterialLayerBlend",
    "Texture2D", "Texture", "VirtualTexture2D", "PCGGraph", "PCGGraphInstance",
    "Blueprint", "World",
}


def categorize_package(package: str) -> str:
    """只有项目源包属于Game；外部依赖必须单独审计，不可悄然拷贝。"""
    if package.startswith("/Game/"):
        return "SampleContent"
    if package.startswith("/Engine/"):
        return "EngineDependency"
    if package.startswith("/Script/"):
        return "NativeModuleDependency"
    if package.startswith("/") and len(package.split("/")) >= 3:
        return "ExternalPluginDependency"
    return "InvalidPackageName"


def select_graph_seeds(package_names: list[str]) -> list[str]:
    """只使用源工程现有森林图作为候选，禁止硬编码虚构资产路径。"""
    forest = sorted({p for p in package_names
                     if p.startswith(GRAPH_ROOT + "/")
                     and "forest" in p.lower()})
    return forest[:MAX_GRAPH_SEEDS]


def validate_project_path(path: Path, workspace_root: Path | None = None) -> None:
    """只允许已下载的独立Electric Dreams工程，不允许对SSLM运行源工程扫描。"""
    if path.suffix.lower() != ".uproject" or not path.is_file():
        raise RuntimeError("缺少已下载的Electric Dreams .uproject源工程")
    if path.name == "DivineBeastsArena.uproject":
        raise RuntimeError("禁止在SSLM正式项目中当作Electric Dreams源工程运行")
    if workspace_root and path.resolve().is_relative_to(workspace_root.resolve()):
        raise RuntimeError("源项目不得存放在SSLM工作空间内")
    if "electricdream" not in path.stem.lower():
        raise RuntimeError("运行的工程名不是预期的Electric Dreams样例，请先人工核实来源")


def collect_inventory(ue) -> dict:
    """使用原生UE资产注册表追踪森林示例引用闭包，不加载或修改其他原生资产。"""
    project = Path(ue.Paths.get_project_file_path().replace("\\", "/"))
    validate_project_path(project)
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    roots = registry.get_assets_by_package_name(ue.Name(FOREST_MAP))
    if not roots:
        raise RuntimeError("已加载源工程中没有官方ElectricDreams_PCGForest地图，停止审计")
    graph_assets = registry.get_assets_by_path(ue.Name(GRAPH_ROOT), recursive=True) or []
    graph_paths = [str(a.package_name) for a in graph_assets
                   if "PCGGraph" in str(a.asset_class_path)]
    graph_seeds = select_graph_seeds(graph_paths)
    if not graph_seeds:
        raise RuntimeError("官方森林示例图路径未能通过AssetRegistry识别，不允许声称能迁移")

    options = ue.AssetRegistryDependencyOptions()
    options.set_editor_property("include_hard_package_references", True)
    options.set_editor_property("include_soft_package_references", True)
    # 游戏引用与编辑器引用均记录供人工筛选，防止遗漏材质或PCG子图。
    for prop in ("include_game_package_references", "include_editor_only_package_references"):
        try:
            options.set_editor_property(prop, True)
        except Exception:
            pass  # UE5.2无该字段；其它两类包引用仍将被枚举。

    pending = deque([FOREST_MAP, *graph_seeds])
    visited: set[str] = set()
    while pending:
        package = pending.popleft()
        if package in visited:
            continue
        if len(visited) >= MAX_PACKAGES:
            raise RuntimeError("AssetRegistry依赖闭包超过硬上限，拒绝输出不完整迁移建议")
        if categorize_package(package) == "InvalidPackageName":
            raise RuntimeError("发现非法的UE资产引用：" + package)
        visited.add(package)
        for dep in registry.get_dependencies(ue.Name(package), options) or []:
            name = str(dep)
            if name not in visited:
                pending.append(name)

    classes = Counter()
    assets: list[dict] = []
    missing_packages: list[str] = []
    external_packages: list[str] = []
    for pkg in sorted(visited):
        category = categorize_package(pkg)
        if category != "SampleContent":
            external_packages.append(pkg)
            continue
        data = registry.get_assets_by_package_name(ue.Name(pkg)) or []
        if not data:
            missing_packages.append(pkg)
            continue
        for asset in data:
            cls = str(asset.asset_class_path).split(".")[-1]
            classes[cls] += 1
            assets.append({
                "package": str(asset.package_name),
                "assetName": str(asset.asset_name),
                "assetClass": cls,
                "vfxOrArtCandidate": cls in ASSET_CLASSES,
            })
    if missing_packages:
        raise RuntimeError("官方森林依赖闭包缺少AssetRegistry记录：" + ", ".join(missing_packages[:10]))

    return {
        "schemaVersion": 1,
        "task": "SSLM Electric Dreams UE-only forest migration provenance audit",
        "sourceProject": str(project),
        "sourceForestLevel": FOREST_MAP,
        "sourceForestGraphSeeds": graph_seeds,
        "assetRegistryScanSucceeded": True,
        "fabOwnershipChecked": False,
        "sourceAssetModified": False,
        "migrationExecuted": False,
        "targetVillageModified": False,
        "targetThirdPartyMount": "/DBAWorldPack_Village/Environment/ThirdParty/ElectricDreams",
        "dependencyPackageCount": len(visited),
        "sourceAssetCount": len(assets),
        "sourceClassCounts": dict(sorted(classes.items())),
        "externalReferenceCount": len(external_packages),
        "externalReferences": external_packages,
        "assets": assets,
        "disposition": "read_only_inventory_requires_licensed_operator_and_ue_migrate",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="UE编辑器内部Electric Dreams森林依赖只读清单")
    parser.add_argument("--mode", choices=("inspect", "audit"), default="inspect")
    parser.add_argument("--report-name", default="ForestSourceDependencies",
                        help="输出文件名基础名，仅允许ASCII字母数字及下划线")
    args = parser.parse_args()
    if args.mode == "inspect":
        print("ELECTRIC_DREAMS_AUDIT_PLAN", FOREST_MAP, GRAPH_ROOT)
        print("未在官方源UE工程内执行audit，不包含Fab权益检查与资产迁移")
        return 0
    if not args.report_name.isascii() or not args.report_name.replace("_", "").isalnum():
        raise ValueError("报告名称只能使用ASCII字母、数字和下划线")
    import unreal as ue  # type: ignore
    inventory = collect_inventory(ue)
    saved = Path(ue.Paths.project_saved_dir().replace("\\", "/")).resolve()
    output_root = saved / "Validation" / "ElectricDreamsSource"
    output_root.mkdir(parents=True, exist_ok=True)
    name = args.report_name + "_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".json"
    target = output_root / name
    if target.exists():
        raise FileExistsError("审计报告不可覆盖：" + str(target))
    target.write_text(json.dumps(inventory, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print("ELECTRIC_DREAMS_FOREST_SOURCE_AUDITED", inventory["dependencyPackageCount"])
    print("ELECTRIC_DREAMS_FOREST_SOURCE_REPORT", target)
    print("FAB_ENTITLEMENT_NOT_VERIFIED_NO_MIGRATION_PERFORMED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
