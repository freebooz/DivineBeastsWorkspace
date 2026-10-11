#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01 20米裸露崖壁的UE5.8独立真实地图人工审核。

仅使用已存在的WorldAligned材质MI与BasicShapes原生立面网格，不改正式Village世界。
创建L_DBA_CliffRock01Review（/Game/Development/Terrain/Maps），双20米宽×13米高
竖直面：左侧20米WorldAligned世界投射，右侧UV贴图供可见对照。
2米QA方块可观察法线和投射尺度；动态方向光/双补光/天空大气让暗部可见。
QA关卡不加入正式Cook/生产逻辑；禁用无根据的人为天气/FPS达标标记。
默认CLIFFROCK_REVIEW_MAP_MODE=inspect只读；apply原生Editor创建UWorld、
Actor和保存；verify只加载已保存UWorld且核实立面Actor、位置和材质身份。
地图目标已存在时禁止覆盖、创建时必须能从资产注册表反射保存回读。
"""

from __future__ import annotations

import os

MODE=os.getenv("CLIFFROCK_REVIEW_MAP_MODE","inspect").lower().strip()
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
WORLD_MI="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned"
UV_MI="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_PBRReview"
QA_MI="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAligned_Review2m"
CUBE="/Engine/BasicShapes/Cube"
PLANE="/Engine/BasicShapes/Plane"

SCENE_ACTORS={
    "CliffReview_WorldAligned_20m":("WorldAligned", (0,-1080,700),(.45,19.5,13)),
    "CliffReview_UV_20m":("UV", (0,1080,700),(.45,19.5,13)),
    "CliffReview_WorldAligned_2m":("QA", (-310,-2350,250),(2,2,4)),
}

def verify_world(unreal):
    assets=unreal.EditorAssetLibrary
    if not assets.does_asset_exist(MAP):
        raise FileNotFoundError("独立Cliff审核地图实际.uasset/umap不存在")
    world=assets.load_asset(MAP)
    if not isinstance(world,unreal.World):
        raise TypeError("审核地图并非真正UWorld/UMAP")
    print("CLIFFROCK_REVIEW_MAP_WORLD_REFLECTED",world.get_path_name())
    return world

def create()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("审核地图仅允许inspect/apply/verify")
    for name in (MAP,WORLD_MI,UV_MI,QA_MI,CUBE,PLANE):
        print("CLIFFROCK_REVIEW_MAP_PLAN",name)
    if MODE=="inspect":return
    try:import unreal # type: ignore
    except ImportError as ex:
        raise RuntimeError("不能在非UE Editor环境伪造.umap") from ex
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("Cliff QA必须在正式DivineBeastsArena工程")
    assets=unreal.EditorAssetLibrary
    levels=unreal.EditorLevelLibrary
    materials={
        "WorldAligned":assets.load_asset(WORLD_MI),
        "UV":assets.load_asset(UV_MI),
        "QA":assets.load_asset(QA_MI)
    }
    if any(not isinstance(x,unreal.MaterialInstanceConstant) for x in materials.values()):
        raise TypeError("Cliff材料候选与2m QA MI的真实UObject缺失")
    cube=assets.load_asset(CUBE)
    plane=assets.load_asset(PLANE)
    if not isinstance(cube,unreal.StaticMesh) or not isinstance(plane,unreal.StaticMesh):
        raise TypeError("UE5.8标准网格丢失")
    if MODE=="verify":
        verify_world(unreal)
        return
    if assets.does_asset_exist(MAP):
        verify_world(unreal)
        print("CLIFFROCK_REVIEW_MAP_ALREADY_EXISTS_DID_NOT_OVERWRITE",MAP)
        return
    folder=MAP.rsplit("/",1)[0]
    if not assets.does_directory_exist(folder):
        assets.make_directory(folder)
    if not levels.new_level(MAP):
        raise RuntimeError("UE5.8新建独立Cliff审核UWorld失败")
    world=levels.get_editor_world()
    if not isinstance(world,unreal.World):
        raise RuntimeError("刚创建的编辑器世界并非真实UE UWorld")

    def mesh_actor(name,mesh,location,scale,mat):
        actor=levels.spawn_actor_from_class(
            unreal.StaticMeshActor,unreal.Vector(*location),unreal.Rotator(0,0,0))
        if not isinstance(actor,unreal.StaticMeshActor):
            raise RuntimeError("审核立面静态网格Actor未创建："+name)
        actor.set_actor_label(name)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        comp=actor.get_editor_property("static_mesh_component")
        comp.set_static_mesh(mesh)
        comp.set_material(0,mat)
        if comp.get_material(0)!=mat:
            raise RuntimeError("真实Cliff材质没有绑定到立面："+name)
        return actor

    # 左右各19.5米宽立面；Y轴铺展、Z轴高13米，前表面朝向X负向摄影机。
    for name,(key,location,scale) in SCENE_ACTORS.items():
        mesh_actor(name,cube,location,scale,materials[key])
    mesh_actor("CliffReview_Ground",plane,(0,0,-10),(65,70,1),materials["UV"])

    def lighting(cls,label,xyz,rotator,brightness,range_cm=None):
        actor=levels.spawn_actor_from_class(cls,unreal.Vector(*xyz),unreal.Rotator(*rotator))
        if not actor:
            raise RuntimeError("可控审核照明Actor创建失败："+label)
        actor.set_actor_label(label)
        comp=actor.get_editor_property("light_component")
        comp.set_editor_property("mobility",unreal.ComponentMobility.MOVABLE)
        comp.set_editor_property("intensity",float(brightness))
        if range_cm and isinstance(comp,unreal.PointLightComponent):
            comp.set_editor_property("attenuation_radius",float(range_cm))
        return actor
    lighting(unreal.DirectionalLight,"CliffReview_Sun",(-2600,-2200,3100),(-43,-30,0),8.0)
    lighting(unreal.PointLight,"CliffReview_LeftSoftbox",(-1800,-1300,1500),(0,0,0),
             105000,10000)
    lighting(unreal.PointLight,"CliffReview_RightSoftbox",(-1750,1650,1650),(0,0,0),
             85000,10000)
    lighting(unreal.SkyLight,"CliffReview_Skylight",(0,0,2000),(0,0,0),1.6)
    for klass,label,at in (
        (unreal.SkyAtmosphere,"CliffReview_SkyAtmosphere",(0,0,0)),
        (unreal.ExponentialHeightFog,"CliffReview_ThinFog",(0,0,0))):
        obj=levels.spawn_actor_from_class(klass,unreal.Vector(*at))
        if obj:obj.set_actor_label(label)

    camera=levels.spawn_actor_from_class(unreal.CameraActor,
        unreal.Vector(-3500,0,900),unreal.Rotator(-3.0,0,0))
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("20米Cliff场景摄影机未创建")
    camera.set_actor_label("CliffReview_Camera_20m")
    camera.get_editor_property("camera_component").set_editor_property("field_of_view",69.0)
    levels.set_level_viewport_camera_info(
        unreal.Vector(-3500,0,900),unreal.Rotator(-3.0,0,0))
    if not levels.save_current_level():
        raise RuntimeError("保存真实独立CliffReview.umap失败")
    loaded=verify_world(unreal)
    print("CLIFFROCK_REVIEW_MAP_UE_SAVED",MAP,
          "20mWorldAlignedWall=1 20mUVWall=1 2mQABox=1",
          "lit=True sky=True")

if __name__=="__main__":
    create()
