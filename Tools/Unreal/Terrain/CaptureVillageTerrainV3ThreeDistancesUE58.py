#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM Village正式地图V3自然绿色+三通道Texture Bombing真实D3D12多景别审核。

从真正L_Village_Start地图获取Landscape边界与母材质身份，按远景/中景/近景
捕获1280x720 FinalColorLDR。与上次黑帧不同，用UE FindLookAtRotation生成正确机位，
且严格复用UE启动时已加载World，不在同一帧内重复load_level。

只生成Saved下PNG/审查Manifest；不修改地图、灯光、角色、PCG、材质或Shader。
截图有效≠GPU毫秒、帧率/玩家视角或用户人工美术签字，需另行审核。
"""
from __future__ import annotations
import hashlib
import json
import math
from pathlib import Path

MAP="/DBAWorldPack_Village/Maps/L_Village_Start"
MAT="/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
FALLBACK="/DBAWorldPack_Village/Materials/Landscape/Defaults/T_DBA_Village_GroundAlbedoV3Default"
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/"Game/Saved/Validation/VillageTerrainV3RealD3D12_20261018"
MAPFILE=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
              "Content/Maps/L_Village_Start.umap")
CAMERAS=(
    ("Wide","远景全地形",0.40,-0.58,0.60,58.0),
    ("Medium","中景草地坡面",0.12,-0.20,0.16,64.0),
    ("Close","近景地面纹理",0.02,-0.06,0.07,66.0),
)

def sha(p:Path)->str:
    obj=hashlib.sha256()
    with p.open("rb") as f:
        for buf in iter(lambda:f.read(1<<20),b""):obj.update(buf)
    return obj.hexdigest()

def capture(unreal, world, eye, look, name, fov):
    rot=unreal.MathLibrary.find_look_at_rotation(eye,look)
    vec=unreal.MathLibrary.get_forward_vector(rot)
    delta=unreal.Vector(look.x-eye.x,look.y-eye.y,look.z-eye.z)
    norm=math.sqrt(delta.x**2+delta.y**2+delta.z**2)
    dot=(delta.x*vec.x+delta.y*vec.y+delta.z*vec.z)/norm
    if dot<.999:raise RuntimeError("正式Village相机未瞄准地表："+name)
    actor=None
    try:
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,eye,rot)
        if not isinstance(actor,unreal.SceneCapture2D):
            raise RuntimeError("UE5.8无法创建正式Village场景捕获Actor")
        comp=actor.get_editor_property("capture_component2d")
        rt=unreal.RenderingLibrary.create_render_target2d(
            world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(comp,unreal.SceneCaptureComponent2D) or not isinstance(
                rt,unreal.TextureRenderTarget2D):
            raise RuntimeError("UE场景捕获组件/渲染目标不正确")
        comp.set_editor_property("texture_target",rt)
        comp.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property("capture_every_frame",False)
        comp.set_editor_property("capture_on_movement",False)
        comp.set_editor_property("fov_angle",fov)
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",0.0)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        comp.set_editor_property("post_process_settings",post)
        comp.set_editor_property("post_process_blend_weight",1.0)
        comp.capture_scene()
        path=OUT/(name+"_D3D12_1280x720.png")
        unreal.RenderingLibrary.export_render_target(world,rt,str(OUT),path.name)
        if not path.is_file() or path.stat().st_size<30000:
            raise RuntimeError("正式Village实景图片没有真正保存："+name)
        print("SSLM_VILLAGE_V3_REAL_D3D12_FRAME",name,"bytes",path.stat().st_size,
              "alignment",round(dot,6),flush=True)
        return {"label":name,"filename":path.name,"bytes":path.stat().st_size,
                "sha256":sha(path),"lookAtAlignment":round(dot,6),
                "eye":[eye.x,eye.y,eye.z],"target":[look.x,look.y,look.z]}
    finally:
        if actor is not None:unreal.EditorLevelLibrary.destroy_actor(actor)

def run()->None:
    import unreal # type: ignore
    if not str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不是SSLM UE5.8正式项目")
    if "-nullrhi" in unreal.SystemLibrary.get_command_line().lower():
        raise RuntimeError("NullRHI不能用于真实Village视觉审核")
    lib=unreal.EditorAssetLibrary
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not world or MAP not in world.get_path_name():
        raise RuntimeError("必须在启动UE前预加载正式L_Village_Start，禁止同步加载后空帧取图")
    terrain=[a for a in unreal.EditorLevelLibrary.get_all_level_actors()
             if isinstance(a,unreal.LandscapeProxy)]
    if len(terrain)!=1:
        raise RuntimeError("正式Village Landscape资产数量不是一")
    original=terrain[0].get_editor_property("landscape_material")
    if MAT not in original.get_path_name():
        raise RuntimeError("正式Village意外解绑已批准Texture Bombing母材质")
    albedo=lib.load_asset(FALLBACK)
    if not isinstance(albedo,unreal.Texture2D) or not bool(albedo.get_editor_property("srgb")):
        raise RuntimeError("已导入的共享V3草地默认绿色贴图没有通过UE反射")
    center,extent=terrain[0].get_actor_bounds(only_colliding_components=False)
    if max(extent.x,extent.y)<1000.0:
        raise RuntimeError("Village Landscape边界错误")
    before=sha(MAPFILE)
    OUT.mkdir(parents=True,exist_ok=True)
    expected=[OUT/(name+"_D3D12_1280x720.png") for name,*_ in CAMERAS]
    if any(x.exists() for x in expected) or (OUT/"VillageD3D12Evidence.json").exists():
        raise FileExistsError("正式Village的既有审查截图存在，禁止覆盖历史证据")
    records=[]
    for name,label,xx,yy,zz,fov in CAMERAS:
        d=max(extent.x,extent.y)
        eye=unreal.Vector(center.x+xx*d,center.y+yy*d,
                          center.z+zz*d+max(250.0,extent.z))
        look=unreal.Vector(center.x,center.y,center.z)
        records.append(dict(capture(unreal,world,eye,look,name,fov),descriptionZh=label))
    if sha(MAPFILE)!=before:
        raise RuntimeError("正式Village地图意外发生持久化变化")
    record={
        "origin":"UE5.8 D3D12 SceneCapture2D",
        "map":MAP,"landscapeMaterial":MAT,"defaultGreenTexture":FALLBACK,
        "sourceMapSha256":before,"frames":records,
        "pixelGatePassed":False,"humanApproval":False,"gpuPerformancePassed":False,
        "scopeZh":"正式Village地图三景别现场截图，未加载玩家游戏客户端/未真正证明2K动态纹理软租约"
    }
    (OUT/"VillageD3D12Evidence.json").write_text(
        json.dumps(record,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_VILLAGE_V3_REAL_D3D12_THREE_FRAMES_SAVED",
          len(records),"actual_pixel_qa_required=true",flush=True)

if __name__=="__main__":run()
