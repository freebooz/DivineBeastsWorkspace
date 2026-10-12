#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 Village正式地形雪层桥只读审计：实际加载地图、AssetRegistry依赖和基础MID配置。

检查共享地图仅硬引用自身WorldPack中的中性Material和Default Texture2D；不可硬引用
GamePlatformSurface/GamePlatformVFX/DBAPresentationPack_Core纯客户端资源，否则拒绝标记
为DedicatedServer可复用。真实运行时的MID初始化、玩家天气快照和GPU必须另作独立验证。
只在原生UE Editor执行；不保存或更改地图、蓝图及纹理资产。
"""

from __future__ import annotations

import json
from pathlib import Path

MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"
MATERIAL = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
DEFAULT = "/DBAWorldPack_Village/Materials/Landscape/Defaults/"
FORBIDDEN = ("/GamePlatformSurface/", "/GamePlatformVFX/", "/DBAPresentationPack_Core/")


def run() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须通过锁定UE5.8的原生Editor反射环境执行") from exc
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝检查其他项目：" + project)

    lib = unreal.EditorAssetLibrary
    # UE命令行PythonScriptCommandlet初始AssetRegistry未必已扫描Content-only插件；
    # 同步扫描已挂载WorldPack路径只更新内存发现索引，不创建、覆盖或保存任何资产。
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/DBAWorldPack_Village"], force_rescan=True)
    for path in (MAP, MATERIAL):
        if not lib.does_asset_exist(path):
            raise FileNotFoundError("工程正式资产缺失：" + path)
    master = lib.load_asset(MATERIAL)
    if not isinstance(master, unreal.Material):
        raise TypeError("Village母材质不是真实UMaterial")
    expression_count = unreal.MaterialEditingLibrary.get_num_material_expressions(master)
    if expression_count < 40:
        raise RuntimeError("Village母材质没有完整写实雪视觉节点：" + str(expression_count))
    for name in ("T_DBA_Village_SnowCoverageDefault",
                 "T_DBA_Village_SnowAlbedoDefault",
                 "T_DBA_Village_SnowNormalDefault"):
        asset = DEFAULT + name
        if not isinstance(lib.load_asset(asset), unreal.Texture2D):
            raise TypeError("Village默认中性纹理缺失：" + asset)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    dep_opts = unreal.AssetRegistryDependencyOptions()
    dep_opts.set_editor_property("include_hard_package_references", True)
    dep_opts.set_editor_property("include_soft_package_references", True)
    dep_opts.set_editor_property("include_searchable_names", False)
    dep_opts.set_editor_property("include_hard_management_references", False)
    dep_opts.set_editor_property("include_soft_management_references", False)

    def dependencies(package: str) -> list[str]:
        return sorted({str(v) for v in registry.get_dependencies(package, dep_opts)})

    material_dependencies = dependencies(MATERIAL)
    map_dependencies = dependencies(MAP)
    for category, found in (("LandscapeMaterial", material_dependencies), ("VillageMap", map_dependencies)):
        forbidden = [x for x in found if x.startswith(FORBIDDEN)]
        if forbidden:
            raise RuntimeError("共享地图/材质直接依赖客户端资产：" + category + ": " +
                               ", ".join(forbidden[:8]))
        print("VILLAGE_SNOW_DEPENDENCY_ISOLATION_PASS", category, len(found))
    if not any(x.startswith(DEFAULT) for x in material_dependencies):
        raise RuntimeError("基础Landscape材质没有引用同WorldPack中性Texture2D")

    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not editor.load_level(MAP):
        raise RuntimeError("UE无法重新加载正式Village世界")
    landscapes = [a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                  if isinstance(a, unreal.LandscapeProxy)]
    if len(landscapes) != 1:
        raise RuntimeError("Village Landscape数量异常，不能宣称完成绑定：" + str(len(landscapes)))
    actor = landscapes[0]
    current = actor.get_editor_property("landscape_material")
    if not current or current.get_path_name() != master.get_path_name():
        raise RuntimeError("正式地图的LandscapeMaterial没有保存写实雪桥")
    if not actor.get_editor_property("use_dynamic_material_instance"):
        raise RuntimeError("正式Village尚未开启Landscape动态材质实例")

    evidence = {
        "project": project,
        "world": MAP,
        "material": MATERIAL,
        "materialExpressionCount": expression_count,
        "landscapeCount": len(landscapes),
        "dynamicLandscapeMaterialInstancesEnabled": True,
        "materialPackageDependencies": material_dependencies,
        "worldPackageDependencies": map_dependencies,
        "forbiddenClientOnlyMaterialDependencies": 0,
        "resultBoundaryZh": "确认Editor原生World绑定和无客户端硬依赖；不等于运行时MID生成、Snow可见或Client/Server Cook通过",
    }
    report = Path(project).parent / "Saved/Validation/VillageSnowBridgeAssetAudit_20261011.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(evidence, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print("VILLAGE_SNOW_BRIDGE_ASSET_AUDIT_PASS", report)


if __name__ == "__main__":
    run()
