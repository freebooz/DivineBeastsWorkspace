#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01 20米真实绝壁UE5.8原生SceneCapture2D RenderTarget截图（审核专用）。

不修改正式Village，不覆盖任何PBR资产或新手村场景，不假冒图片生成器输出。
直接由GUI UE5.8 D3D12世界的真实静态网格、3轴材质和可控灯光渲染到
GPU RenderTarget2D，再导出PNG。SceneCapture2D Actor为临时审核对象，
导出后原位销毁，不保存到.umap。不允许在NullRHI或错误地图中执行。
截图完成还必须离线检查文件真实存在、像素分布/高频及可读性才能报告通过。
"""
from __future__ import annotations

from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
OUTPUT=(Path("E:/work/2026/DivineBeastsWorkspace/Game/Saved/Validation/"
             "TerrainGaea/CliffRock01")/"CliffRock01_20m_ActualSceneCaptureGPU.png")


def capture()->None:
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("GPU审核截图必须属于正式神兽联盟主工程")
    if OUTPUT.exists():
        print("CLIFFROCK_SCENECAPTURE_EXISTING_ASSET_DID_NOT_OVERWRITE",OUTPUT)
        return
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("GPU真实截图仅针对独立20m悬崖审核地图")
    camera=next((a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                 if a.get_actor_label()=="CliffReview_Camera_20m"),None)
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("缺少独立20米真实崖壁摄影机")
    target=unreal.RenderingLibrary.create_render_target2d(
        world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8,
        unreal.LinearColor(0,0,0,1),False,False)
    if not isinstance(target,unreal.TextureRenderTarget2D):
        raise TypeError("UE没有创建真正RenderTarget2D GPU纹理")
    obj=None
    try:
        obj=unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.SceneCapture2D,camera.get_actor_location(),camera.get_actor_rotation())
        if not isinstance(obj,unreal.SceneCapture2D):
            raise TypeError("SceneCapture2D审核临时Actor创建失败")
        obj.set_actor_label("CliffReview_TemporaryGPUCapture_DO_NOT_SAVE")
        component=obj.get_editor_property("capture_component2d")
        if not isinstance(component,unreal.SceneCaptureComponent2D):
            raise TypeError("SceneCapture2D没有实际原生捕获组件")
        component.set_editor_property("fov_angle",69.0)
        component.set_editor_property("texture_target",target)
        component.set_editor_property("capture_every_frame",False)
        component.set_editor_property("capture_on_movement",False)
        component.set_editor_property("capture_source",
             unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        component.capture_scene()
        OUTPUT.parent.mkdir(parents=True,exist_ok=True)
        unreal.RenderingLibrary.export_render_target(
            world,target,str(OUTPUT.parent),OUTPUT.name)
        if not OUTPUT.is_file() or OUTPUT.stat().st_size<50000:
            raise RuntimeError("UE真实SceneCapture2D没有产生可审核PNG，不能冒充成功")
        print("CLIFFROCK_20M_GPU_SCENECAPTURE_FILE_SAVED",OUTPUT,
              "bytes",OUTPUT.stat().st_size,"resolution=1920x1080")
    finally:
        if obj:
            unreal.EditorLevelLibrary.destroy_actor(obj)
            print("CLIFFROCK_TRANSIENT_SCENECAPTURE_ACTOR_DESTROYED")
    print("CLIFFROCK_20M_GPU_SCENECAPTURE_ASSET_PASS manualApprovalPending=True")


if __name__=="__main__":
    capture()
