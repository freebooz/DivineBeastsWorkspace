#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 实际天气审核地图生成器。

默认WEATHER_REVIEW_MAP_MODE=inspect，仅输出内容；apply须在正式UE5.8编辑器中。
只创建独立开发审核地图，不修改L_Village_Start或游戏启动地图。
要求真实GameMode和WeatherReviewController蓝图、Surface母材质均已编译保存。
创建有地面、岩石/建筑代理物体、可见光、雾和PlayerStart的可审核实际关卡；
任何目标重名都拒绝覆盖，保存后按UE类型回读。地图不得进入Dedicated Server Cook。
"""
from __future__ import annotations

import os

MODE = os.environ.get("WEATHER_REVIEW_MAP_MODE", "inspect").strip().lower()
MAP = "/Game/Development/Weather/Maps/L_DBA_WeatherReview"
GAMEMODE = "/Game/Development/Weather/Blueprints/BP_DBA_WeatherReviewGameMode"
CONTROLLER = "/Game/Development/Weather/Blueprints/BP_DBA_WeatherReviewController"
SURFACE = "/GamePlatformSurface/Materials/M_GP_Surface_Master"
ENGINE_CUBE = "/Engine/BasicShapes/Cube.Cube"
ENGINE_PLANE = "/Engine/BasicShapes/Plane.Plane"


def main():
    print("WEATHER_REVIEW_MAP", MAP)
    for dep in (GAMEMODE, CONTROLLER, SURFACE, ENGINE_CUBE, ENGINE_PLANE):
        print("REQUIRES", dep)
    if MODE == "inspect":
        print("WEATHER_REVIEW_MAP_INSPECT_ONLY：未生成/覆盖任何.umap")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_REVIEW_MAP_MODE只接受inspect或apply")

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只有在UE5.8真实编辑器才能创建地图；普通Python不可伪造.umap") from exc

    assets = unreal.EditorAssetLibrary
    levels = unreal.EditorLevelLibrary
    if assets.does_asset_exist(MAP):
        raise FileExistsError("天气审核地图已存在，禁止覆盖：" + MAP)
    for dep in (GAMEMODE, CONTROLLER, SURFACE, ENGINE_CUBE, ENGINE_PLANE):
        if not assets.does_asset_exist(dep):
            raise FileNotFoundError("真实UE地图前置资产不存在：" + dep)

    mode_class = assets.load_blueprint_class(GAMEMODE)
    actor_class = assets.load_blueprint_class(CONTROLLER)
    ground_material = assets.load_asset(SURFACE)
    cube = assets.load_asset(ENGINE_CUBE)
    plane = assets.load_asset(ENGINE_PLANE)
    if mode_class is None or actor_class is None:
        raise RuntimeError("实际审核GameMode或Actor蓝图不可加载；请先编译保存两个蓝图")
    if not isinstance(ground_material, unreal.Material):
        raise TypeError("Surface完整母材质非真实UMaterial")
    if not isinstance(cube, unreal.StaticMesh) or not isinstance(plane, unreal.StaticMesh):
        raise TypeError("引擎基础测试网格资产异常")

    folder = MAP.rsplit("/", 1)[0]
    if not assets.does_directory_exist(folder):
        assets.make_directory(folder)
    if not levels.new_level(MAP):
        raise RuntimeError("UE没有成功新建独立天气审核地图")

    world = levels.get_editor_world()
    if world is None:
        raise RuntimeError("无法取得实际UE世界，拒绝报告地图生成成功")
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", mode_class)

    def mesh_actor(name: str, static_mesh, pos, scale):
        actor = levels.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(*pos), unreal.Rotator(0, 0, 0)
        )
        if not isinstance(actor, unreal.StaticMeshActor):
            raise RuntimeError("无法创建真实StaticMeshActor：" + name)
        actor.set_actor_label(name)
        mesh_component = actor.get_editor_property("static_mesh_component")
        mesh_component.set_static_mesh(static_mesh)
        mesh_component.set_material(0, ground_material)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    # 地面与四个不同倾斜测试块；坡度/湿润/积雪可在同一画面审核。
    mesh_actor("WeatherReview_Ground", plane, (0, 0, -20), (55, 55, 1))
    mesh_actor("WeatherReview_RockA", cube, (-700, -450, 160), (3.2, 3.2, 4))
    mesh_actor("WeatherReview_RockB", cube, (600, -390, 90), (4, 2.5, 1.7))
    mesh_actor("WeatherReview_RoofProxy", cube, (400, 650, 360), (7, 4, .23))

    player = levels.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-1000, -1200, 160),
        unreal.Rotator(0, 45, 0))
    if player is None:
        raise RuntimeError("审核地图没有生成PlayerStart")

    for light_class, pos in (
        (unreal.DirectionalLight, (0, 0, 1400)),
        (unreal.SkyLight, (0, 0, 1000)),
        (unreal.SkyAtmosphere, (0, 0, 700)),
        (unreal.ExponentialHeightFog, (0, 0, 0)),
    ):
        actor = levels.spawn_actor_from_class(light_class, unreal.Vector(*pos))
        if actor is None:
            raise RuntimeError("天气审核环境灯光或雾Actor缺失：" + str(light_class))

    review = levels.spawn_actor_from_class(actor_class, unreal.Vector(0, 0, 150))
    if review is None:
        raise RuntimeError("天气审核Actor蓝图实际生成失败")
    review.set_actor_label("WeatherReview_AuthorityOnly")
    # 默认bApplyAtBeginPlay=false，避免预览地图载入时重复覆盖权威天气。
    if not levels.save_current_level():
        raise RuntimeError("UE没有保存真实天气审核地图")
    path = assets.load_asset(MAP)
    if path is None or not isinstance(path, unreal.World):
        raise RuntimeError("保存后审核地图无法按UWorld回读")
    print("WEATHER_REVIEW_UE_MAP_SAVED", MAP)


if __name__ == "__main__":
    main()
