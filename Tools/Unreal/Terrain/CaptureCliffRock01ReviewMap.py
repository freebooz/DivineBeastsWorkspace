#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 ClifRock01独立20米崖壁审核关卡：申请一帧真实Editor GPU截图。

此脚本仅由真正GUI UnrealEditor.exe + D3D12 RHI执行；不会生成概念图，
不会修改正式Village/临时材质或第三方纹理；只打开已有独立审核地图并设相机。
AutomationLibrary.take_high_res_screenshot()为引擎延迟任务，调度成功 != 已保存GPU图像，
必须事后检查目标文件实际存在/打开视觉核验才可报告成功。
"""
from __future__ import annotations

from pathlib import Path

MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
OUTPUT=(Path("E:/work/2026/DivineBeastsWorkspace/Game/Saved/Validation/"
             "TerrainGaea/CliffRock01")/"CliffRock01_20m_RealUEViewport.png")


def main():
    import unreal  # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("Cliff截图必须在正式神兽联盟UE工程")
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP):
        raise FileNotFoundError("20米崖壁实际UWorld地图未创建")
    if OUTPUT.exists():
        print("CLIFFROCK_EXISTING_SCREENSHOT_NOT_OVERWRITTEN",OUTPUT)
        return
    if not unreal.EditorLevelLibrary.load_level(MAP):
        raise RuntimeError("Editor不能打开Cliff审核地图")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World):
        raise RuntimeError("真实Editor世界未加载")
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    camera=next((a for a in actors if a.get_actor_label()=="CliffReview_Camera_20m"),None)
    if not isinstance(camera,unreal.CameraActor):
        raise RuntimeError("20米崖壁审核摄影机不存在")
    for required in ("CliffReview_WorldAligned_20m","CliffReview_UV_20m",
                     "CliffReview_LeftSoftbox","CliffReview_RightSoftbox"):
        if not any(a.get_actor_label()==required for a in actors):
            raise RuntimeError("真实物理尺寸岩壁/可控照明缺失："+required)
    loc=camera.get_actor_location()
    rot=camera.get_actor_rotation()
    unreal.EditorLevelLibrary.set_level_viewport_camera_info(loc,rot)
    print("CLIFFROCK_20M_REVIEW_CAMERA_ACTIVE",loc,rot)
    OUTPUT.parent.mkdir(parents=True,exist_ok=True)
    task=unreal.AutomationLibrary.take_high_res_screenshot(
        1920,1080,str(OUTPUT),camera=camera,delay=4.0,force_game_view=True)
    if not task:
        raise RuntimeError("UE没有接受高分辨率真实渲染截图任务")
    print("CLIFFROCK_20M_REAL_UE_GPU_SCREENSHOT_SCHEDULED",OUTPUT,
          "note=must_verify_written_pixels_later")
    # 不在这里宣告截屏成功；真结果在UE后续Tick由Capture任务异步完成。
    return task


if __name__=="__main__":
    main()
