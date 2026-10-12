#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟第三层独立苔藓/雪景人工审查地图作者脚本。

新地图只放在Development/MossSnow，不触碰Village共享地图与服务器Cook。
默认inspect只读；MOSS_REVIEW_MAP_MODE=apply才请求UE编辑器创建真实.umap。
"""
import os

MODE=os.getenv("MOSS_REVIEW_MAP_MODE","inspect").strip().lower()
MAP="/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview"
MATERIAL="/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff"
CONTROLLER="/Game/Development/Weather/Blueprints/BP_DBA_WeatherReviewController"
CUBE="/Engine/BasicShapes/Cube"
PLANE="/Engine/BasicShapes/Plane"

def run():
    if MODE not in ("inspect","verify","apply"):
        raise ValueError("MOSS_REVIEW_MAP_MODE只允许inspect/verify/apply")
    for asset in (MAP,MATERIAL,CONTROLLER,CUBE,PLANE):print("MOSS_REVIEW_TARGET",asset)
    if MODE=="inspect": return
    import unreal
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许官方DivineBeastsArena编辑器")

    assets=unreal.EditorAssetLibrary
    # 与命令行资产制作保持一致，先同步枚举插件和Developemnt目录。
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/Game","/DBAPresentationPack_Core"],force_rescan=True)
    if assets.does_asset_exist(MAP):
        if not isinstance(assets.load_asset(MAP),unreal.World):
            raise TypeError("审核地图同名资产不是World")
        print("MOSS_REVIEW_UE_MAP_PRESENT",MAP)
        return
    if MODE=="verify": raise FileNotFoundError("独立苔藓雪景审核地图还未创建")

    for dep in (MATERIAL,CONTROLLER,CUBE,PLANE):
        if not assets.does_asset_exist(dep): raise FileNotFoundError("缺少审查资产："+dep)
    mat=assets.load_asset(MATERIAL)
    cube=assets.load_asset(CUBE)
    plane=assets.load_asset(PLANE)
    controller_cls=assets.load_blueprint_class(CONTROLLER)
    game_mode=getattr(unreal,"DivineBeastsWorldGameMode",None)
    if not isinstance(mat,unreal.MaterialInstanceConstant) or not isinstance(cube,unreal.StaticMesh):
        raise RuntimeError("项目材质实例或立方体原生资源不正确")

    if not isinstance(plane,unreal.StaticMesh) or controller_cls is None or game_mode is None:
        raise RuntimeError("缺少审核平面/天气控制蓝图/项目游戏模式")
    levels=unreal.EditorLevelLibrary
    folder=MAP.rsplit("/",1)[0]
    if not assets.does_directory_exist(folder): assets.make_directory(folder)
    if not levels.new_level(MAP): raise RuntimeError("UE无法创建苔藓雪景.umap")
    world=levels.get_editor_world()
    if not world: raise RuntimeError("UE地图World实例不存在")
    world.get_world_settings().set_editor_property("default_game_mode",game_mode)

    def add_mesh(name,mesh,location,scale,pitch=0):
        actor=levels.spawn_actor_from_class(
            unreal.StaticMeshActor,unreal.Vector(*location),unreal.Rotator(pitch,0,0))
        if not isinstance(actor,unreal.StaticMeshActor):
            raise RuntimeError("UE无法创建审核网格："+name)
        actor.set_actor_label(name)
        comp=actor.get_editor_property("static_mesh_component")
        comp.set_static_mesh(mesh); comp.set_material(0,mat)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        return actor

    # 地面/25度/50度/垂直岩壁四种几何坡度，在PIE比较苔藓和积雪。
    add_mesh("MossSnowReview_Ground",plane,(0,0,-30),(62,62,1))
    add_mesh("MossSnowReview_RockFlat",cube,(-500,300,40),(3.2,2.5,1))
    add_mesh("MossSnowReview_Slope25",cube,(440,330,90),(3.1,2.5,.24),25)
    add_mesh("MossSnowReview_Slope50",cube,(450,-450,120),(3.1,2.5,.24),50)
    add_mesh("MossSnowReview_CliffWall",cube,(-550,-440,210),(.35,3.1,3.9))
    start=levels.spawn_actor_from_class(unreal.PlayerStart,
        unreal.Vector(-970,-1140,175),unreal.Rotator(0,44,0))
    if start is None: raise RuntimeError("UE无法创建审核起点")

    for cls,location in ((unreal.DirectionalLight,(0,0,2100)),
                         (unreal.SkyLight,(0,0,1200)),
                         (unreal.SkyAtmosphere,(0,0,800)),
                         (unreal.ExponentialHeightFog,(0,0,0))):
        if levels.spawn_actor_from_class(cls,unreal.Vector(*location)) is None:
            raise RuntimeError("UE无法生成独立场景光照")

    authority=levels.spawn_actor_from_class(controller_cls,unreal.Vector(0,0,200))
    if authority is None: raise RuntimeError("天气演示蓝图生成失败")
    authority.set_actor_label("MossSnowReview_WeatherAuthority")
    authority.set_editor_property("apply_at_begin_play",True)
    authority.set_editor_property("preview_weather",unreal.GamePlatformWeatherType.HEAVY_SNOW)
    authority.set_editor_property("preview_transition_seconds",12.0)
    if not levels.save_current_level(): raise RuntimeError("独立苔藓雪景地图保存失败")
    if not isinstance(assets.load_asset(MAP),unreal.World):
        raise RuntimeError("地图保存后回读不是真实World")
    print("MOSS_SNOW_REVIEW_MAP_SAVED",MAP)

if __name__=="__main__":
    run()
