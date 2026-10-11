#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01独立已保存地图按材质启动D3D12审查，无运行时材质替换。

baseline与lowWarp在两个互不相干的UE进程，读取相同原地图复制品和同相机照明。
各捕获2次以检查Shader/Mip驻留稳定性；从不覆盖先前截图，不保存审核地图；
不触碰正式Village、旧Rock01或生产MI。
"""
from pathlib import Path
import os

ROOT=Path(__file__).resolve().parents[3]
STREAM_MODE=os.getenv("CLIFF_TEXTURE_STREAMING_MODE","off").strip().lower()
if STREAM_MODE not in ("on","off"):
    raise ValueError("纹理流送校准模式只能on或off")
RUN_TAG=os.getenv("CLIFF_CAPTURE_RUN_TAG","").strip()
if RUN_TAG not in ("","FullPlugins","GUIWarm"):
    raise ValueError("仅允许已登记的FullPlugins或GUIWarm审核批次，拒绝路径注入")
OUT=ROOT/("Game/Saved/Validation/TerrainGaea/CliffRock01LowWarpStreamingOn"+RUN_TAG+"20261011"
          if STREAM_MODE=="on" else
          "Game/Saved/Validation/TerrainGaea/CliffRock01LowWarpIsolated20261011")
PREFIX="/Game/Development/Terrain/Maps"
CHOOSE={
    "baseline":(PREFIX+"/L_DBA_CliffRock01LowWarpBaselineReview",
               "/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned"),
    "lowWarp":(PREFIX+"/L_DBA_CliffRock01LowWarpVariantReview",
               "/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAlignedMacro_LowWarpReview"),
}
UV="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview"
LIGHTS={"CliffReview_Sun":2.2,"CliffReview_LeftSoftbox":13000.,
        "CliffReview_RightSoftbox":10000.,"CliffReview_Skylight":.62}
CHOSEN=os.getenv("CLIFF_ISOLATED_CAPTURE_VARIANT","")

def main():
    import unreal
    if CHOSEN not in CHOOSE:
        raise ValueError("必须明确选择baseline或lowWarp QA地图")
    path,material=CHOOSE[CHOSEN]
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("只能在DivineBeastsArena正式工程进行孤立GPU测试")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or path not in world.get_path_name():
        del world
        if not unreal.EditorLevelLibrary.load_level(path):
            raise RuntimeError("UE无法加载已保存独立材质QA地图："+path)
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or path not in world.get_path_name():
        raise RuntimeError("当前UWorld错误，拒绝截图")
    actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    cam=actors.get("CliffReview_Camera_20m")
    left=actors.get("CliffReview_WorldAligned_20m")
    right=actors.get("CliffReview_UV_20m")
    if not isinstance(cam,unreal.CameraActor) or not isinstance(left,unreal.StaticMeshActor):
        raise RuntimeError("独立20米测试网格、相机错误")
    if not isinstance(right,unreal.StaticMeshActor):
        raise RuntimeError("UV对照网格缺失")
    mat=unreal.EditorAssetLibrary.load_asset(material)
    uv=unreal.EditorAssetLibrary.load_asset(UV)
    if (not isinstance(mat,unreal.MaterialInstanceConstant) or
        not isinstance(uv,unreal.MaterialInstanceConstant) or
        left.get_editor_property("static_mesh_component").get_material(0)!=mat or
        right.get_editor_property("static_mesh_component").get_material(0)!=uv):
        raise RuntimeError("审核地图中的持久化Material引用未正确保存")
    photos=[OUT/("CliffLowWarp_"+CHOSEN+"_D3D12_"+str(i)+".png") for i in range(2)]
    if any(f.exists() for f in photos):
        raise FileExistsError("同类真实GPU截图已经存在，不能覆盖")
    lights={}
    initial={}
    for name in LIGHTS:
        a=actors.get(name)
        if not a:raise RuntimeError("审核关卡固定照明Actor缺失："+name)
        comp=a.get_editor_property("light_component")
        lights[name]=comp
        initial[name]=float(comp.get_editor_property("intensity"))
    prior_stream=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")
    actor=None
    try:
        if STREAM_MODE=="off":
            unreal.SystemLibrary.execute_console_command(world,"r.TextureStreaming 0")
            if unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")!=0:
                raise RuntimeError("此模式要求纹理Streaming关闭")
        else:
            if prior_stream!=1:
                raise RuntimeError("Streaming On对照进程起始CVar不是1，拒绝污染实验")
        print("CLIFF_LOW_ISOLATED_GPU_STREAMING",CHOSEN,STREAM_MODE,
              "current",unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming"))
        for name,level in LIGHTS.items():
            comp=lights[name]
            setter=getattr(comp,"set_intensity",None)
            if setter:setter(level)
            else:comp.set_editor_property("intensity",level)
        actor=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,cam.get_actor_location(),cam.get_actor_rotation())
        if not isinstance(actor,unreal.SceneCapture2D):
            raise RuntimeError("GPU真实SceneCapture2D创建失败")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(target,unreal.TextureRenderTarget2D):
            raise RuntimeError("缺少RGBA8 RenderTarget")
        capture=actor.get_editor_property("capture_component2d")
        capture.set_editor_property("texture_target",target)
        capture.set_editor_property("fov_angle",69.)
        capture.set_editor_property("capture_every_frame",False)
        capture.set_editor_property("capture_on_movement",False)
        capture.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",-2.)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        capture.set_editor_property("post_process_settings",post)
        capture.set_editor_property("post_process_blend_weight",1.)
        OUT.mkdir(parents=True,exist_ok=True)
        for index,f in enumerate(photos):
            for _ in range(8):
                capture.capture_scene()
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),f.name)
            if not f.is_file() or f.stat().st_size<60000:
                raise RuntimeError("GPU输出PNG太小/未产生："+str(f))
            print("CLIFF_LOW_ISOLATED_REAL_D3D12_CAPTURE",CHOSEN,index,f.name,f.stat().st_size)
    finally:
        for key,comp in lights.items():
            setter=getattr(comp,"set_intensity",None)
            if setter:setter(initial[key])
            else:comp.set_editor_property("intensity",initial[key])
        if actor is not None:
            unreal.EditorLevelLibrary.destroy_actor(actor)
        if STREAM_MODE=="off":
            unreal.SystemLibrary.execute_console_command(world,
                "r.TextureStreaming "+str(prior_stream))
        print("CLIFF_LOW_ISOLATED_GPU_LIGHTING_AND_TEMP_ACTOR_RESTORED",CHOSEN)
    print("CLIFF_LOW_ISOLATED_GPU_TWO_FRAMES_PASS",CHOSEN,
          "manual_approval_pending=True")

if __name__=="__main__":
    main()
