#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01：只在当前UE编辑器进程临时降低审核光照以获取可用GPU场景证据。

不修改SourceArt、WorldAligned PBR材质、正式Village和已保存的QA.umap。
针对20m双立面场景，临时把Sun/Skylight/两盏PointLight降低，再通过真实
D3D12 SceneCapture2D输出两组PNG，捕获结束在finally内恢复原LightComponent
参数并销毁临时Actor。画面须经真实像素检查和人工审阅后才能称为美术验收。
"""
from __future__ import annotations
from pathlib import Path

TARGET=Path("E:/work/2026/DivineBeastsWorkspace/Game/Saved/Validation/TerrainGaea/CliffRock01")
LIGHTS={
    "CliffReview_Sun":(2.2,1.0),
    "CliffReview_LeftSoftbox":(13000.0,5000.0),
    "CliffReview_RightSoftbox":(10000.0,4000.0),
    "CliffReview_Skylight":(.62,.36)
}

def main():
    import unreal  # type: ignore
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or "L_DBA_CliffRock01Review" not in world.get_path_name():
        raise RuntimeError("当前不是独立Cliff20米审核世界，不允许改灯光")
    actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
    camera=actors.get("CliffReview_Camera_20m")
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("审核相机不存在")
    comps={}
    previous={}
    for name in LIGHTS:
        obj=actors.get(name)
        if not obj:
            raise RuntimeError("审核灯光Actor缺失："+name)
        light=obj.get_editor_property("light_component")
        comps[name]=light
        previous[name]=float(light.get_editor_property("intensity"))
    cap=unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
    if not isinstance(cap,unreal.SceneCapture2D):
        raise RuntimeError("SceneCapture2D原生场景捕获Actor创建失败")
    target=unreal.RenderingLibrary.create_render_target2d(
        world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
        unreal.LinearColor(0,0,0,1),False,False)
    cc=cap.get_editor_property("capture_component2d")
    cc.set_editor_property("texture_target",target)
    cc.set_editor_property("fov_angle",69.0)
    cc.set_editor_property("capture_every_frame",False)
    cc.set_editor_property("capture_on_movement",False)
    cc.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    # 同一临时捕获摄像机锁定手动曝光，避免暗天幕干扰场景HDR眼睛适应。
    post=unreal.PostProcessSettings()
    post.set_editor_property("override_auto_exposure_method",True)
    post.set_editor_property("auto_exposure_method",unreal.AutoExposureMethod.AEM_MANUAL)
    post.set_editor_property("override_auto_exposure_bias",True)
    post.set_editor_property("auto_exposure_bias",-2.0)
    post.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
    post.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
    cc.set_editor_property("post_process_settings",post)
    cc.set_editor_property("post_process_blend_weight",1.0)

    try:
        TARGET.mkdir(parents=True,exist_ok=True)
        for index,(name,tag) in enumerate((("Balanced","13kAnd10k"),("Soft","5kAnd4k"))):
            path=TARGET/("CliffRock01_20m_"+name+"Lights_ActualGPU.png")
            if path.exists():
                raise FileExistsError("不会覆盖旧的真实GPU审核图："+str(path))
            for actor_name,options in LIGHTS.items():
                target_val=options[index]
                light=comps[actor_name]
                setter=getattr(light,"set_intensity",None)
                if setter:
                    setter(float(target_val))
                else:
                    light.set_editor_property("intensity",float(target_val))
                val=float(light.get_editor_property("intensity"))
                if abs(val-target_val)>.01:
                    raise RuntimeError("UE审核灯光设置未生效："+actor_name)
            cc.capture_scene()
            unreal.RenderingLibrary.export_render_target(world,target,str(TARGET),path.name)
            if not path.is_file() or path.stat().st_size<60000:
                raise RuntimeError("真实GPU图未有效落盘："+str(path))
            print("CLIFFROCK_20M_BALANCED_LIT_CAPTURE_SAVED",name,str(path),path.stat().st_size)
    finally:
        for actor_name,light in comps.items():
            setter=getattr(light,"set_intensity",None)
            if setter:
                setter(previous[actor_name])
            else:
                light.set_editor_property("intensity",previous[actor_name])
        unreal.EditorLevelLibrary.destroy_actor(cap)
        for actor_name,light in comps.items():
            if abs(float(light.get_editor_property("intensity"))-previous[actor_name])>.01:
                raise RuntimeError("临时审核灯光未恢复，禁止签收："+actor_name)
        print("CLIFFROCK_REVIEW_LIGHT_INTENSITIES_RESTORED_NONE_SAVED")
    print("CLIFFROCK_20M_LIT_GPU_AUDIT_CAPTURE_PASS manualApprovalPending=True")

if __name__=="__main__":
    main()
