#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01真实20米崖壁场景：固定曝光UE GPU RenderTarget对照采样。

仅对临时SceneCapture2DComponent应用UE后处理手动曝光，不修改原始岩壁材质、
其他项目世界、正式Village或用户的Monolith配置；真实D3D12引擎World渲染。
先测试-2EV/-3EV，获得不过曝的青灰岩壁、对照UV材质和真实三向投射阴影。
生成文件后仍需直接人工查看PNG并检查亮度分布；技术捕获不是AAA签收。
"""
from __future__ import annotations

from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
ROOT=Path("E:/work/2026/DivineBeastsWorkspace/Game/Saved/Validation/TerrainGaea/CliffRock01")

def main():
    import unreal  # type: ignore
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("不能在错误关卡捕获世界绝壁")
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    camera=next((x for x in actors if x.get_actor_label()=="CliffReview_Camera_20m"),None)
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("Cliff 20m审核摄影机不存在")
    tar=unreal.RenderingLibrary.create_render_target2d(
        world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
        unreal.LinearColor(0,0,0,1),False,False)
    if not isinstance(tar,unreal.TextureRenderTarget2D):
        raise RuntimeError("UE5.8真实GPU RenderTarget未创建")
    capture=unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
    if not isinstance(capture,unreal.SceneCapture2D):
        raise RuntimeError("临时审核捕获Actor未创建")
    try:
        component=capture.get_editor_property("capture_component2d")
        component.set_editor_property("texture_target",tar)
        component.set_editor_property("fov_angle",69.0)
        component.set_editor_property("capture_every_frame",False)
        component.set_editor_property("capture_on_movement",False)
        component.set_editor_property("capture_source",
                                      unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        ROOT.mkdir(parents=True,exist_ok=True)
        for exposure in (-2.0,-3.0):
            dest=ROOT/("CliffRock01_20m_ManualEV"+str(abs(int(exposure)))+"_ActualGPU.png")
            if dest.exists():
                print("CLIFFROCK_BALANCED_SCENE_ALREADY_EXISTS_NO_OVERWRITE",dest)
                continue
            settings=unreal.PostProcessSettings()
            settings.set_editor_property("override_auto_exposure_method",True)
            settings.set_editor_property("auto_exposure_method",
                                         unreal.AutoExposureMethod.AEM_MANUAL)
            settings.set_editor_property("override_auto_exposure_bias",True)
            settings.set_editor_property("auto_exposure_bias",float(exposure))
            settings.set_editor_property("override_auto_exposure_apply_physical_camera_exposure",True)
            settings.set_editor_property("auto_exposure_apply_physical_camera_exposure",False)
            component.set_editor_property("post_process_settings",settings)
            component.set_editor_property("post_process_blend_weight",1.0)
            component.capture_scene()
            unreal.RenderingLibrary.export_render_target(world,tar,str(ROOT),dest.name)
            if not dest.is_file() or dest.stat().st_size<50000:
                raise RuntimeError("UE真实固定曝光RenderTarget没有输出有效PNG："+str(dest))
            print("CLIFFROCK_MANUAL_EXPOSURE_GPU_CAPTURE_SAVED",exposure,
                  dest,"bytes",dest.stat().st_size)
    finally:
        unreal.EditorLevelLibrary.destroy_actor(capture)
        print("CLIFFROCK_BALANCED_CAPTURE_ACTOR_DESTROYED")
    print("CLIFFROCK_MANUAL_EXPOSURE_REAL_GPU_SAMPLES_PASS times=2 still_need_visual_signoff")


if __name__=="__main__":
    main()
