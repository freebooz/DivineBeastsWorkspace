#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01 真实D3D12宏观绝壁：独立无纹理流送全分辨率复审。

用于分析此前UE Editor-Cmd刚启动即SceneCapture出现的全局纹理模糊问题。
仅作用本次进程：r.TextureStreaming=0，尝试临时强制已加载Cliff四张4K
Texture2D高分辨Mip驻留；重复SceneCapture预热，再保存本轮全新PNG。
审核地图是独立MacroReview，绝不加载/修改正式Village或其它正在使用的Editor。
所有临时灯光强度在finally恢复，SceneCapture临时Actor在finally销毁。
本脚本只证明已生成真实GPU帧；必须由独立像素/人工检查判定是否清晰。
"""

from __future__ import annotations

from pathlib import Path
import time

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
OUT=ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011"
LIGHTS={
    "CliffReview_Sun":(2.2,1.0),
    "CliffReview_LeftSoftbox":(13000.,5000.),
    "CliffReview_RightSoftbox":(10000.,4000.),
    "CliffReview_Skylight":(.62,.36)
}
OUTPUTS=(
    "CliffRock01Macro_20m_BalancedLights_FullMip_ActualGPU.png",
    "CliffRock01Macro_20m_SoftLights_FullMip_ActualGPU.png",
)
PBR_ROOT="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Textures"
PBR_NAMES=(
    "T_DBA_CliffRock01_BaseColor_BlueGray",
    "T_DBA_CliffRock01_NormalDX",
    "T_DBA_CliffRock01_ORM",
    "T_DBA_CliffRock01_Height",
)


def main()->None:
    import unreal # type: ignore
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("当前不是独立CliffMacroReview地图，拒绝GPU操作")
    for file in OUTPUTS:
        if (OUT/file).exists():
            raise FileExistsError("已有完整源GPU截图，不覆盖："+file)
    loaded=[]
    for name in PBR_NAMES:
        tex=unreal.EditorAssetLibrary.load_asset(PBR_ROOT+"/"+name)
        if not isinstance(tex,unreal.Texture2D):
            raise RuntimeError("Cliff原生Texture2D丢失："+name)
        if tex.blueprint_get_size_x()!=4096 or tex.blueprint_get_size_y()!=4096:
            raise RuntimeError("扫描4K纹理被错误缩小："+name)
        loaded.append(tex)
        resident=getattr(tex,"set_force_mip_levels_to_be_resident",None)
        if resident:
            try:
                resident(120.,False)
                print("CLIFF_MACRO_GPU_FORCE_FULLMIP_TEXTURE",name)
            except Exception as ex:
                print("CLIFF_MACRO_GPU_FORCE_MIP_API_UNAVAILABLE",name,str(ex)[:160])
    before=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")
    unreal.SystemLibrary.execute_console_command(world,"r.TextureStreaming 0")
    after=unreal.SystemLibrary.get_console_variable_int_value("r.TextureStreaming")
    print("CLIFF_MACRO_GPU_STREAMING_STATE",before,after)
    if after!=0:
        raise RuntimeError("没有在本次测试进程关闭纹理流送，拒绝误称全分辨率")
    actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    camera=actors.get("CliffReview_Camera_20m")
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("Cliff20m真实审核摄影机不存在")
    expected={
        "CliffReview_WorldAligned_20m":"/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro",
        "CliffReview_UV_20m":"/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview",
    }
    for name,assetpath in expected.items():
        actor=actors.get(name)
        mi=unreal.EditorAssetLibrary.load_asset(assetpath)
        if not isinstance(actor,unreal.StaticMeshActor) or not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("20米宏观或UV审核网格缺失："+name)
        if actor.get_editor_property("static_mesh_component").get_material(0)!=mi:
            raise RuntimeError("正在审核的板材Material被其他任务改变："+name)
    previous={}
    comps={}
    for name in LIGHTS:
        actor=actors.get(name)
        if not actor:raise RuntimeError("审核光源缺失："+name)
        comp=actor.get_editor_property("light_component")
        previous[name]=float(comp.get_editor_property("intensity"))
        comps[name]=comp
    capture=None
    try:
        capture=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
        if not isinstance(capture,unreal.SceneCapture2D):
            raise RuntimeError("SceneCapture2D未创建")
        target=unreal.RenderingLibrary.create_render_target2d(
            world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
            unreal.LinearColor(0,0,0,1),False,False)
        if not isinstance(target,unreal.TextureRenderTarget2D):
            raise RuntimeError("D3D12真实GPU RenderTarget不存在")
        component=capture.get_editor_property("capture_component2d")
        component.set_editor_property("texture_target",target)
        component.set_editor_property("fov_angle",69.)
        component.set_editor_property("capture_every_frame",False)
        component.set_editor_property("capture_on_movement",False)
        component.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        post=unreal.PostProcessSettings()
        post.set_editor_property("override_auto_exposure_method",True)
        post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
        post.set_editor_property("override_auto_exposure_bias",True)
        post.set_editor_property("auto_exposure_bias",-2.)
        post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
        post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
        component.set_editor_property("post_process_settings",post)
        component.set_editor_property("post_process_blend_weight",1.)
        OUT.mkdir(parents=True,exist_ok=True)
        for index,filename in enumerate(OUTPUTS):
            for key,values in LIGHTS.items():
                comp=comps[key]
                val=float(values[index])
                setter=getattr(comp,"set_intensity",None)
                if setter:setter(val)
                else:comp.set_editor_property("intensity",val)
                if abs(float(comp.get_editor_property("intensity"))-val)>.03:
                    raise RuntimeError("灯光不是批准的平衡/柔和两档："+key)
            for attempt in range(6):
                component.capture_scene()
                if attempt==0:
                    print("CLIFF_MACRO_GPU_FIRST_WARMUP_CAPTURE",index)
            dst=OUT/filename
            unreal.RenderingLibrary.export_render_target(world,target,str(OUT),dst.name)
            if not dst.is_file() or dst.stat().st_size<65000:
                raise RuntimeError("真正UE5.8 GPU高分辨率PNG不存在："+filename)
            print("CLIFF_MACRO_GPU_FULLMIP_CAPTURE_SAVED",filename,"bytes",dst.stat().st_size)
    finally:
        for key,comp in comps.items():
            setter=getattr(comp,"set_intensity",None)
            if setter:setter(previous[key])
            else:comp.set_editor_property("intensity",previous[key])
        if capture is not None:
            unreal.EditorLevelLibrary.destroy_actor(capture)
        for key,comp in comps.items():
            if abs(float(comp.get_editor_property("intensity"))-previous[key])>.03:
                raise RuntimeError("Cliff审核临时灯光没有恢复")
        print("CLIFF_MACRO_GPU_FULLMIP_LIGHTS_AND_ACTOR_RESTORED")
    print("CLIFF_MACRO_GPU_FULLMIP_PROBE_PASS",
          "streaming=0","images=2","userApprovalPending=True")


if __name__=="__main__":
    main()
