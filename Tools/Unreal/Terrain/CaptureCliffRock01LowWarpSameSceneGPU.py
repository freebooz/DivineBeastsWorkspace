#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01：同D3D12进程同20米场景，原始vs低振幅vs原宏观GPU审查。

只操作独立MacroReview临时Actor材质、临时SceneCapture2D与光源；finally恢复且绝不保存关卡。
拍摄baseline、lowwarp、fullwarp、baselineRepeat，用两次baseline判断本次Shader/Mip漂移。
不覆盖历史PNG或项目正式Village，不宣称场景性能/真人验收已通过。
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01LowWarp20261011"
BASE="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned"
LOW="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAlignedMacro_LowWarpReview"
FULL="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro"
UV="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview"
TESTS=(
    ("Baseline","CliffLowWarp_00_Baseline_20m_D3D12.png",BASE),
    ("LowWarp","CliffLowWarp_01_LowWarp_20m_D3D12.png",LOW),
    ("FullWarp","CliffLowWarp_02_FullWarp_20m_D3D12.png",FULL),
    ("BaselineRepeat","CliffLowWarp_03_BaselineRepeat_20m_D3D12.png",BASE),
)
LIGHTS={"CliffReview_Sun":2.2,"CliffReview_LeftSoftbox":13000.,
        "CliffReview_RightSoftbox":10000.,"CliffReview_Skylight":.62}

def main():
    import unreal
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("需要神兽联盟唯一正式uproject")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        del world
        if not unreal.EditorLevelLibrary.load_level(MAP):
            raise RuntimeError("CliffMacroReview开发地图未能真实加载")
        world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("不能在正式Village或其他编辑器世界中截屏")
    for _,filename,_ in TESTS:
        if (OUT/filename).exists():
            raise FileExistsError("禁止覆盖GPU证据："+filename)
    actors={actor.get_actor_label():actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()}
    camera=actors.get("CliffReview_Camera_20m")
    lhs=actors.get("CliffReview_WorldAligned_20m")
    rhs=actors.get("CliffReview_UV_20m")
    if (not isinstance(camera,unreal.CameraActor) or
            not isinstance(lhs,unreal.StaticMeshActor) or
            not isinstance(rhs,unreal.StaticMeshActor)):
        raise RuntimeError("20米测试网格/相机不存在")
    comp=lhs.get_editor_property("static_mesh_component")
    uvcomp=rhs.get_editor_property("static_mesh_component")
    mats={path:unreal.EditorAssetLibrary.load_asset(path) for path in (BASE,LOW,FULL,UV)}
    if any(not isinstance(obj,unreal.MaterialInstanceConstant) for obj in mats.values()):
        raise RuntimeError("宏观或基准UE材质实例没有从磁盘正确加载")
    if comp.get_material(0)!=mats[FULL] or uvcomp.get_material(0)!=mats[UV]:
        raise RuntimeError("独立审核关卡材质已经被其他任务改变")
    mi=mats[LOW]
    parent=mi.get_editor_property("parent")
    if parent!=mats[FULL].get_editor_property("parent"):
        raise RuntimeError("低振幅必须共享原宏观母材质")
    scale=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
        mi,"CliffWorldTextureSizeCm")
    amp=unreal.MaterialEditingLibrary.get_material_instance_vector_parameter_value(
        mi,"CliffMacroWarpAmplitudeCm")
    if (abs(scale.r-2000)>1 or abs(amp.r-20)>.1 or
            abs(amp.g+15)>.1 or abs(amp.b-12)>.1):
        raise RuntimeError("真实UE低振幅MI物理尺度/扰动与审核合同不符")
    lights={}
    before={}
    for label in LIGHTS:
        actor=actors.get(label)
        if actor is None:
            raise RuntimeError("Cliff原定灯光缺失："+label)
        lights[label]=actor.get_editor_property("light_component")
        before[label]=float(lights[label].get_editor_property("intensity"))
    prev=comp.get_material(0)
    original_stream=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")
    capture=None
    try:
        unreal.SystemLibrary.execute_console_command(world,"r.TextureStreaming 0")
        state=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")
        if state!=0:
            raise RuntimeError("GPU对照无法控制纹理Mip Streaming")
        for name,level in LIGHTS.items():
            light=lights[name]
            setter=getattr(light,"set_intensity",None)
            if setter:setter(level)
            else:light.set_editor_property("intensity",level)
        capture=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
        if not isinstance(capture,unreal.SceneCapture2D):
            raise RuntimeError("GPU SceneCapture2D创建失败")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(target,unreal.TextureRenderTarget2D):
            raise RuntimeError("UE纹理输出RenderTarget未创建")
        cap=capture.get_editor_property("capture_component2d")
        cap.set_editor_property("texture_target",target)
        cap.set_editor_property("fov_angle",69.)
        cap.set_editor_property("capture_every_frame",False)
        cap.set_editor_property("capture_on_movement",False)
        cap.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",-2.)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        cap.set_editor_property("post_process_settings",post)
        cap.set_editor_property("post_process_blend_weight",1.)
        OUT.mkdir(parents=True,exist_ok=True)
        for tag,filename,material in TESTS:
            comp.set_material(0,mats[material])
            if comp.get_material(0)!=mats[material] or uvcomp.get_material(0)!=mats[UV]:
                raise RuntimeError("同场景材质引用不能回读："+tag)
            for _ in range(8):
                cap.capture_scene()
            photo=OUT/filename
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),photo.name)
            if not photo.is_file() or photo.stat().st_size<60000:
                raise RuntimeError("UE5.8 D3D12有效PNG不存在："+tag)
            print("CLIFF_LOWWARP_GPU_SAME_SCENE_CAPTURED",tag,photo.stat().st_size,photo)
    finally:
        comp.set_material(0,prev)
        if uvcomp.get_material(0)!=mats[UV]:
            uvcomp.set_material(0,mats[UV])
        for name,light in lights.items():
            set_intensity=getattr(light,"set_intensity",None)
            if set_intensity:set_intensity(before[name])
            else:light.set_editor_property("intensity",before[name])
        if capture is not None:
            unreal.EditorLevelLibrary.destroy_actor(capture)
        if original_stream!=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming"):
            unreal.SystemLibrary.execute_console_command(world,"r.TextureStreaming "+str(original_stream))
        if comp.get_material(0)!=prev or uvcomp.get_material(0)!=mats[UV]:
            raise RuntimeError("低扰动审核后的原材质没有恢复")
        if any(abs(float(lights[n].get_editor_property("intensity"))-v)>.1 for n,v in before.items()):
            raise RuntimeError("低扰动审核光照未恢复")
        print("CLIFF_LOWWARP_SCENE_MATERIAL_LIGHTS_RESTORED")
    print("CLIFF_LOWWARP_D3D12_FOUR_FRAME_CAPTURE_PASS",
          "baseline=2 lowWarp=1 fullWarp=1 userApprovalPending=True")

if __name__=="__main__":
    main()
