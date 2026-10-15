#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Electric Dreams官方PCG森林的隔离UE工程迁移入口（保留完整依赖）。

归属：SSLM第三层项目资源创作辅助工具；GamePlatformPCG不依赖具体游戏素材。
前提：通过Epic/Fab已授权取得完整官方样例，先复制一个可回退源工程；
另用UE编辑器创建空的SSLM_ElectricDreams_Staging.uproject作为独立临时工程。
输入：在官方示例UE编辑器中运行本脚本，参数stage-project指定空的临时项目。
输出：先做只读安全预检；仅--mode apply在显式授权确认后调用官方MigratePackages，
      把地图和森林Graph及所有依赖复制进临时工程的Content目录。
限制：不将素材导入SSLM正式Game/Content，也不自动修改Village或覆盖任何目标资产。
      源文件在磁盘上且迁移成功仍不证明Fab账号权限、目标项目AssetRegistry、
      Cook或实际森林视觉结果；后续须UE内修复引用和归类到项目层内容包。
"""
from __future__ import annotations

import argparse
from datetime import datetime
import json
from pathlib import Path
from typing import Any

SOURCE_MAP = "/Game/Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest"
SOURCE_GRAPH = "/Game/PCG/Graphs/Forest/PCGDemo_Forest"
STAGING_PROJECT_FILENAME = "SSLM_ElectricDreams_Staging.uproject"
PACKAGE_MAGIC_LE = bytes((0xC1, 0x83, 0x2A, 0x9E))
MIGRATE_PATHS = (
    "Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest.umap",
    "PCG/Graphs/Forest/PCGDemo_Forest.uasset",
)
WORKSPACE_PROJECT = "DivineBeastsArena.uproject"


def validate_staging(source: Path, stage: Path) -> dict[str, Any]:
    """失败关闭：来源/临时工程必须分离，且首次迁移前临时Content完全没有UE资产。"""
    source = source.resolve()
    stage = stage.resolve()
    if not source.is_file() or source.suffix.lower() != ".uproject":
        raise FileNotFoundError("Electric Dreams来源工程不是真实uproject")
    if "electricdream" not in source.stem.lower() or source.name == WORKSPACE_PROJECT:
        raise RuntimeError("来源工程必须为Electric Dreams官方示例的独立工程")
    if stage.name != STAGING_PROJECT_FILENAME or not stage.is_file():
        raise FileNotFoundError("必须先创建单独的SSLM_ElectricDreams_Staging.uproject临时工程")
    if source == stage or source.parent == stage.parent:
        raise RuntimeError("不得把来源工程和迁移临时工程混用")
    if any(parent.name == "DivineBeastsWorkspace" for parent in stage.parents):
        raise RuntimeError("临时迁移工程不能放入SSLM公开代码工作空间")
    if any(parent.name == "DivineBeastsWorkspace" for parent in source.parents):
        raise RuntimeError("样例来源不能是SSLM工作区内的复制资产")
    for project in (source, stage):
        try:
            data = json.loads(project.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError, UnicodeDecodeError) as e:
            raise ValueError("独立UE项目配置无法解析：" + str(project)) from e
        if not isinstance(data, dict) or not isinstance(data.get("FileVersion"), int):
            raise ValueError("独立工程缺少有效FileVersion：" + str(project))
    content = stage.parent / "Content"
    if not content.is_dir():
        raise FileNotFoundError("临时工程Content目录不存在；必须用UE生成空模板")
    present = [p for p in content.rglob("*")
               if p.is_file() and p.suffix.lower() in (".uasset", ".umap")]
    return {
        "sourceProject": str(source),
        "stagingProject": str(stage),
        "destinationContent": str(content),
        "stageAssetCount": len(present),
        "stageEmpty": not present,
        "sourceContentExists": (source.parent / "Content").is_dir(),
    }


def native_package_present(content: Path, relative: str) -> bool:
    """只读检查UE包头，不能替代UE资产注册表反射加载与依赖核验。"""
    candidate = content / relative
    if not candidate.is_file() or candidate.stat().st_size < 64:
        return False
    with candidate.open("rb") as reader:
        return reader.read(4) == PACKAGE_MAGIC_LE


def validate_source(ue) -> None:
    """只有真正加载官方样例且AssetRegistry发现两个既定源包才可迁移。"""
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    for package in (SOURCE_MAP, SOURCE_GRAPH):
        found = registry.get_assets_by_package_name(ue.Name(package))
        if not found:
            raise RuntimeError("AssetRegistry缺失官方PCG资源：" + package)


def execute(mode: str, stage_project: Path | None, acknowledge_license: bool) -> dict[str, Any]:
    """apply仅迁移至独立空工程，禁止直接操作SSLM正式Content。"""
    if mode not in ("inspect", "apply", "verify"):
        raise ValueError("不支持的模式")
    if mode == "inspect" and stage_project is None:
        return {"mode": "inspect", "sourcePackages": [SOURCE_MAP, SOURCE_GRAPH],
                "stageProjectRequired": STAGING_PROJECT_FILENAME,
                "migrationExecuted": False, "villageMapModified": False}
    import unreal as ue  # type: ignore  # 必须由真实UE Editor运行。
    source = Path(ue.Paths.get_project_file_path().replace("\\", "/"))
    if stage_project is None:
        raise ValueError("请显式指定--stage-project隔离临时工程")
    state = validate_staging(source, stage_project)
    content = Path(state["destinationContent"])
    state.update({"mode": mode, "migrationExecuted": False,
                  "villageMapModified": False, "fabEntitlementApiVerified": False})
    if mode == "inspect":
        return state
    validate_source(ue)
    if mode == "verify":
        state["nativePackagesPresent"] = {
            relative: native_package_present(content, relative)
            for relative in MIGRATE_PATHS
        }
        state["nativePackagesVerified"] = all(state["nativePackagesPresent"].values())
        return state
    if not acknowledge_license:
        raise PermissionError("先通过Epic/Fab官方账户取得授权，并显式确认UE-Only许可")
    if not state["stageEmpty"]:
        raise FileExistsError("临时工程已有UE资产，拒绝自动迁移或覆盖既存数据")
    if not state["sourceContentExists"]:
        raise FileNotFoundError("来源工程缺少Content")
    options = ue.MigrationOptions()
    options.set_editor_property("prompt", False)
    options.set_editor_property("ignore_dependencies", False)
    options.set_editor_property("asset_conflict", ue.AssetMigrationConflict.SKIP)
    tools = ue.AssetToolsHelpers.get_asset_tools()
    tools.migrate_packages([ue.Name(SOURCE_MAP), ue.Name(SOURCE_GRAPH)],
                           str(content), options)
    # 官方迁移API无成功返回值：必须回读真实文件，失败即拒绝宣告完成。
    result = {relative: native_package_present(content, relative)
              for relative in MIGRATE_PATHS}
    state["nativePackagesPresent"] = result
    state["nativePackagesVerified"] = all(result.values())
    if not state["nativePackagesVerified"]:
        raise RuntimeError("UE MigratePackages执行后缺少官方森林地图或Graph，需原生迁移日志审查")
    state["migrationExecuted"] = True
    state["migrationDestination"] = "isolated_staging_only"
    state["nextAction"] = "open_staging_project_verify_all_dependencies_and_remap_into_village_contentpack"
    return state


def main() -> int:
    parser = argparse.ArgumentParser(description="UE5.8 Electric Dreams森林保留依赖的隔离迁移")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"), default="inspect")
    parser.add_argument("--stage-project", type=Path, default=None)
    parser.add_argument("--acknowledge-ue-only-license", action="store_true")
    args = parser.parse_args()
    result = execute(args.mode, args.stage_project, args.acknowledge_ue_only_license)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if args.mode == "apply":
        print("ELECTRIC_DREAMS_STAGING_DEPENDENCY_MIGRATION_DONE_NOT_IMPORTED_TO_SSLM")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
