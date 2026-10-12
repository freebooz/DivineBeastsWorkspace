#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8中真实创建或回读六项Village PCG非发行技术模型审查关卡。

项目层内容，绝不写Foundation模板和正式Village地图。仅使用已从OBJ导入、
保存、回读并核对尺寸的6个StaticMesh，创建独立Development审查地图。
它不需要Gaea、Landscape或真实3A美术，但也不声称G01-G16和Cook通过。
"""
from __future__ import annotations
import os

MODE = os.getenv("VILLAGE_PCG_TECH_REVIEW_MODE", "inspect").lower().strip()
MAP = "/DBAWorldPack_Village/Development/PCG/Maps/L_DBA_PCG_TechMeshReview"
MESHES = (
    ("ForestCanopy", -3900), ("RockScatter", -2400),
    ("AgriCrop", -900), ("FieldFence", 900),
    ("FarmGate", 2400), ("WoodBridge", 3900),
)


def run() -> None:
    if MODE not in ("inspect", "apply", "verify"):
        raise ValueError("Village技术审查关卡仅支持inspect/apply/verify")
    print("VILLAGE_PCG_TECH_REVIEW_MAP", MODE, MAP, "mesh_count", len(MESHES))
    if MODE == "inspect":
        return
    try:
        import unreal  # type: ignore
    except ImportError as err:
        raise RuntimeError("只能由真正UE5.8 Editor创建和审查地图，不接受文本伪.umap") from err
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE编辑器没有加载正式《神兽联盟》项目")
    if "-VillagePCGTechMesh" not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError("只能在受控原型导入/审查编辑器中运行本任务")
    library = unreal.EditorAssetLibrary
    levels = unreal.EditorLevelLibrary
    loaded = {}
    for role, _ in MESHES:
        path = "/DBAWorldPack_Village/Development/PCG/Meshes/SM_DBA_PCG_" + role + "_Tech"
        asset = library.load_asset(path)
        if not isinstance(asset, unreal.StaticMesh):
            raise RuntimeError("真实技术StaticMesh尚未保存/反射回读：" + path)
        loaded[role] = asset
    if MODE == "verify":
        if not library.does_asset_exist(MAP) or not levels.load_level(MAP):
            raise FileNotFoundError("独立技术审核关卡未保存或无法加载")
        labels = {
            actor.get_actor_label(): actor
            for actor in levels.get_all_level_actors()
            if actor and isinstance(actor, unreal.StaticMeshActor)
        }
        for role, _ in MESHES:
            label = "PCG_Tech_" + role
            actor = labels.get(label)
            expected = loaded[role]
            if actor is None or actor.static_mesh_component.static_mesh != expected:
                raise RuntimeError("独立地图审查Actor/真正网格引用缺失：" + label)
        print("UE_PCG_SIX_REAL_STATIC_MESHES_MAP_REOPEN_OK", MAP)
        print("UE_EDITOR_MAP_ONLY_GAEA_NOT_REQUIRED_G01_G16_PENDING")
        return

    if library.does_asset_exist(MAP):
        raise FileExistsError("技术审查地图已存在，禁止覆盖或增量迁移：" + MAP)
    package = MAP.rsplit("/", 1)[0]
    if not library.does_directory_exist(package):
        library.make_directory(package)
    if not levels.new_level(MAP):
        raise RuntimeError("UE5.8不能创建实际独立开发地图")
    for role, x in MESHES:
        actor = levels.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(float(x), 0, 15))
        if actor is None:
            raise RuntimeError("SpawnActor失败：" + role)
        actor.set_actor_label("PCG_Tech_" + role)
        actor.static_mesh_component.set_static_mesh(loaded[role])
        # 独立技术审查地图只用于观察模型外形与尺寸，不能把一个大包围盒当成门洞/桥面权威碰撞。
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        actor.static_mesh_component.set_editor_property("generate_overlap_events", False)
    for clazz, label, height in (
        (unreal.DirectionalLight, "PCG_Tech_Sun", 1400),
        (unreal.SkyLight, "PCG_Tech_Sky", 1300),
        (unreal.PlayerStart, "PCG_Tech_ReviewStart", 200),
    ):
        actor = levels.spawn_actor_from_class(
            clazz, unreal.Vector(0, -900, float(height)))
        if actor is None:
            raise RuntimeError("技术审查环境Actor创建失败：" + label)
        actor.set_actor_label(label)
    if not levels.save_current_level():
        raise RuntimeError("UE5.8不能保存技术原型审核UWorld")
    if not isinstance(library.load_asset(MAP), unreal.World):
        raise RuntimeError("已保存技术审查地图不能从AssetRegistry回读UWorld")
    print("UE_PCG_SIX_TECH_MESHES_REAL_MAP_SAVED", MAP)


if __name__ == "__main__":
    run()
