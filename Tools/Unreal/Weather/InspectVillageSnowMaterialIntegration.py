#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""审查神兽联盟正式Village地图的雪层材质可接入范围（真实UE5.8 Editor资产扫描）。

所属：工程工具Tools/Unreal/Weather（编辑器资产清点）。
输入：现有/DBAWorldPack_Village/Maps/L_Village_Start世界资源及其外置Actor。
输出：仅Game/Saved/Validation/SnowVillageMaterialInventory_20261011.json（可重建审计数据）。
运行：由UnrealEditor-Cmd -ExecutePythonScript显式调用，独立进程，不触碰用户其他Editor。
约束：绝不修改地图、替换材质、保存包、执行PIE或修改天气权威；加载/类型异常明确记录。
统计物体实际材质插槽与Landscape材质路径，帮助决定第三层World包的实例归属。
"""
from __future__ import annotations

import json
from collections import Counter
from pathlib import Path

WORLD_PACKAGE = "/DBAWorldPack_Village/Maps/L_Village_Start"
CHECKED_MATERIAL_PREFIXES = ("/GamePlatformSurface/", "/DBAPresentationPack_Core/")


def main() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("本工具要求UE5.8真实编辑器反射环境") from exc
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前编辑器不是正式神兽联盟工程：" + project)
    assets = unreal.EditorAssetLibrary
    if not assets.does_asset_exist(WORLD_PACKAGE):
        raise FileNotFoundError("正式Village地图资产缺失：" + WORLD_PACKAGE)
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels or not levels.load_level(WORLD_PACKAGE):
        raise RuntimeError("无法在独立Editor审查进程加载正式Village地图")

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    type_counts = Counter()
    material_slots = Counter()
    materials = Counter()
    landscapes = []
    samples = []
    for actor in actors:
        type_counts[actor.get_class().get_name()] += 1
        label = actor.get_actor_label()
        if isinstance(actor, unreal.LandscapeProxy):
            try:
                material = actor.get_editor_property("landscape_material")
                mat_path = material.get_path_name() if material else ""
                landscapes.append({"actor": label, "material": mat_path})
                if mat_path:
                    materials[mat_path] += 1
            except Exception as exc:
                landscapes.append({"actor": label, "readError": str(exc)[:250]})
        if isinstance(actor, unreal.StaticMeshActor):
            mesh_component = actor.get_editor_property("static_mesh_component")
            if not mesh_component:
                continue
            for index in range(mesh_component.get_num_materials()):
                obj = mesh_component.get_material(index)
                path = obj.get_path_name() if obj else ""
                material_slots["StaticMesh"] += 1
                if path:
                    materials[path] += 1
                if len(samples) < 40:
                    samples.append({"actor": label, "slot": index, "material": path})

    details = {
        "project": project,
        "world": WORLD_PACKAGE,
        "totalActors": len(actors),
        "actorTypes": dict(type_counts.most_common()),
        "landscapes": landscapes,
        "staticMeshSlots": material_slots["StaticMesh"],
        "topMaterials": [{"path": path, "slots": count} for path, count in materials.most_common(40)],
        "surfaceMaterials": [
            {"path": path, "slots": count} for path, count in materials.items()
            if any(path.startswith(prefix) for prefix in CHECKED_MATERIAL_PREFIXES)
        ],
        "actorSamples": samples,
        "resultMeaning": "只读Editor资产盘点，不等同已替换正式地图的全部地形/植被材质",
    }
    path = Path(project).resolve().parent / "Saved/Validation/SnowVillageMaterialInventory_20261011.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(details, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("SNOW_VILLAGE_AUDIT_PATH", str(path))
    print("SNOW_VILLAGE_TOTAL_ACTORS", len(actors))
    print("SNOW_VILLAGE_LANDSCAPES", len(landscapes))
    print("SNOW_VILLAGE_STATIC_MESH_SLOTS", material_slots["StaticMesh"])
    print("SNOW_VILLAGE_SURFACE_MATERIAL_MATCH", len(details["surfaceMaterials"]))
    for land in landscapes:
        print("SNOW_VILLAGE_LANDSCAPE_MATERIAL", land.get("actor"), land.get("material", land.get("readError", "")))
    print("SNOW_VILLAGE_READ_ONLY_AUDIT_COMPLETE")


if __name__ == "__main__":
    main()
