#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""项目专属独立积雪效果审核地图（实际UE5.8 Editor生成的.umap）。

所属：第三层神兽联盟开发审核关卡；不写入正式新手村或服务器Cook。
复用平台雪母材质的第三层MI、现有天气审核蓝图和UE引擎基本网格。
提供水平地面、直立墙体及倾斜石板，以便审查坡度/高度遮罩和PBR。
默认inspect；SNOW_REVIEW_MAP_MODE=apply才实际调用UE Editor。
"""

from __future__ import annotations

import os

MODE = os.getenv("SNOW_REVIEW_MAP_MODE", "inspect").strip().lower()
MAP = "/Game/Development/Snow/Maps/L_DBA_SnowReview"
CONTROLLER = "/Game/Development/Weather/Blueprints/BP_DBA_WeatherReviewController"
MATERIAL = "/DBAPresentationPack_Core/Materials/Snow/MI_DBA_Snow_Detailed"
PLANE = "/Engine/BasicShapes/Plane"
CUBE = "/Engine/BasicShapes/Cube"


def execute() -> None:
    for target in (MAP, CONTROLLER, MATERIAL, PLANE, CUBE):
        print("SNOW_REVIEW_PLAN", target)
    if MODE == "inspect":
        return
    if MODE != "apply":
        raise ValueError("SNOW_REVIEW_MAP_MODE只允许inspect/apply")
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("UE原生.umap只能由真实Editor创建") from exc

    assets = unreal.EditorAssetLibrary
    levels = unreal.EditorLevelLibrary
    if assets.does_asset_exist(MAP):
        existing = assets.load_asset(MAP)
        if not isinstance(existing, unreal.World):
            raise TypeError("已有审查资产不是UWorld：" + MAP)
        print("SNOW_REVIEW_MAP_ALREADY_EXISTS", MAP)
        return
    for required in (CONTROLLER, MATERIAL, PLANE, CUBE):
        if not assets.does_asset_exist(required):
            raise FileNotFoundError("缺少审查地图前置资产：" + required)
    actor_cls = assets.load_blueprint_class(CONTROLLER)
    gm_cls = getattr(unreal, "DivineBeastsWorldGameMode", None)
    mat = assets.load_asset(MATERIAL)
    plane = assets.load_asset(PLANE)
    cube = assets.load_asset(CUBE)
    if actor_cls is None or gm_cls is None:
        raise TypeError("天气审核蓝图/项目权威GameMode未加载")
    if not isinstance(mat, unreal.MaterialInstanceConstant):
        raise TypeError("项目雪MI不是实际UMaterialInstanceConstant")
    if not isinstance(plane, unreal.StaticMesh) or not isinstance(cube, unreal.StaticMesh):
        raise TypeError("引擎标准测试网格缺失")

    folder = MAP.rsplit("/", 1)[0]
    if not assets.does_directory_exist(folder):
        assets.make_directory(folder)
    if not levels.new_level(MAP):
        raise RuntimeError("UE没有创建独立积雪效果审核关卡")
    world = levels.get_editor_world()
    if not world:
        raise RuntimeError("Editor世界不存在")
    world.get_world_settings().set_editor_property("default_game_mode", gm_cls)

    def add_mesh(label: str, mesh, pos, scale, pitch=0.0):
        actor = levels.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*pos), unreal.Rotator(pitch, 0, 0))
        if not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError("无法创建审核物体：" + label)
        actor.set_actor_label(label)
        component = actor.get_editor_property("static_mesh_component")
        component.set_static_mesh(mesh)
        component.set_material(0, mat)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    # 真实材质遮罩依顶点世界法线，竖立墙和坡面必须在同一审核画面可比较。
    add_mesh("SnowReview_Ground", plane, (0, 0, -30), (62, 62, 1))
    add_mesh("SnowReview_StoneHorizontal", cube, (-480, 280, 40), (3.1, 2.5, 1.1))
    add_mesh("SnowReview_Slope25", cube, (480, 380, 80), (3.3, 2.4, .18), 25)
    add_mesh("SnowReview_Slope50", cube, (480, -450, 80), (3.3, 2.4, .18), 50)
    add_mesh("SnowReview_Vertical", cube, (-580, -390, 200), (.24, 3.2, 3.8))

    start = levels.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-950, -1050, 180),
        unreal.Rotator(0, 45, 0))
    if start is None:
        raise RuntimeError("PlayerStart创建失败")

    for klass, pos in ((unreal.DirectionalLight, (0, 0, 2000)),
                       (unreal.SkyLight, (0, 0, 1400)),
                       (unreal.SkyAtmosphere, (0, 0, 800)),
                       (unreal.ExponentialHeightFog, (0, 0, 0))):
        if not levels.spawn_actor_from_class(klass, unreal.Vector(*pos)):
            raise RuntimeError("无法创建审核环境光/雾：" + str(klass))

    controller = levels.spawn_actor_from_class(actor_cls, unreal.Vector(0, 0, 200))
    if controller is None:
        raise RuntimeError("已有天气审核蓝图Actor无法实例化")
    controller.set_actor_label("SnowReview_WeatherAuthority")
    # 保持审核Controller默认不自动修改服务器天气，审查员在PIE明确选LightSnow/HeavySnow。
    if not levels.save_current_level():
        raise RuntimeError("无法保存真正的积雪审查.umap")
    loaded = assets.load_asset(MAP)
    if not isinstance(loaded, unreal.World):
        raise RuntimeError("审核地图保存后回读UWorld失败")
    print("SNOW_REVIEW_UE_MAP_SAVED", MAP)


if __name__ == "__main__":
    execute()
