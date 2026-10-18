#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3地形真实编辑器视口跨帧D3D12截图试验。

与先前失败的Editor-Cmd同步SceneCapture不同：由普通UnrealEditor.exe打开QA地图后，
在Editor Slate post tick约90帧后使用Editor Viewport原生截图；脚本由 -ExecCmds=py 运行，
不能以 -ExecutePythonScript 执行，因为后者脚本返回即退出UE，无法等待世界渲染完成。
只修改当前临时视口机位，不保存关卡/材质/资产；文件落盘后仍须真实像素检测。
"""
from __future__ import annotations
import math
from pathlib import Path

import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
OUT=ROOT/"Game/Saved/Validation/TerrainV3ViewportTicked_20261018"
PNG=OUT/"TerrainV3_AfterRenderedFrames_1280x720.png"
FRAME=0
PREPARED=False
REQUESTED=False
LAST_REPORT=0
HANDLE=None

if PNG.exists():
    raise FileExistsError("已有相同视口GPU证据图，绝不覆盖："+str(PNG))
OUT.mkdir(parents=True,exist_ok=True)

def stop():
    global HANDLE
    if HANDLE is not None:
        unreal.unregister_slate_post_tick_callback(HANDLE)
        HANDLE=None
    print("SSLM_TERRAIN_V3_VIEWPORT_TICKED_END screenshot_present="+str(PNG.is_file()),
          "pixel_validation_required=true",flush=True)
    world=unreal.EditorLevelLibrary.get_editor_world()
    if world:
        unreal.SystemLibrary.execute_console_command(world,"QUIT")


def tick(_delta):
    global FRAME,PREPARED,REQUESTED,LAST_REPORT
    FRAME+=1
    if FRAME%80==0 and FRAME!=LAST_REPORT:
        LAST_REPORT=FRAME
        print("SSLM_TERRAIN_V3_VIEWPORT_TICK",FRAME,flush=True)
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not world or MAP not in world.get_path_name():
        if FRAME>=480:
            print("SSLM_TERRAIN_V3_VIEWPORT_MAP_NOT_OPEN no_approval=true",flush=True)
            stop()
        return
    if not PREPARED and FRAME>=24:
        targets=[a for a in unreal.EditorLevelLibrary.get_all_level_actors()
                 if isinstance(a,unreal.LandscapeProxy)]
        if len(targets)!=1:
            print("SSLM_TERRAIN_V3_VIEWPORT_LANDSCAPE_WRONG_COUNT",len(targets),flush=True)
            stop();return
        o,e=targets[0].get_actor_bounds(only_colliding_components=False)
        d=max(e.x,e.y)
        pos=unreal.Vector(o.x+0.35*d,o.y-0.53*d,o.z+0.85*d+e.z)
        dx,dy,dz=o.x-pos.x,o.y-pos.y,o.z-pos.z
        rot=unreal.Rotator(math.degrees(math.atan2(dz,math.hypot(dx,dy))),
                           math.degrees(math.atan2(dy,dx)),0)
        unreal.EditorLevelLibrary.set_level_viewport_camera_info(pos,rot)
        unreal.SystemLibrary.execute_console_command(world,"r.EyeAdaptationQuality 0")
        PREPARED=True
        print("SSLM_TERRAIN_V3_VIEWPORT_CAMERA_READY frame="+str(FRAME)+" world="+str(world.get_path_name()),flush=True)
    if PREPARED and not REQUESTED and FRAME>=155:
        try:
            unreal.AutomationLibrary.take_high_res_screenshot(
                1280,720,str(PNG))
        except Exception as exc:
            print("SSLM_TERRAIN_V3_VIEWPORT_SCREENSHOT_API_ERROR",repr(exc),flush=True)
            stop();return
        REQUESTED=True
        print("SSLM_TERRAIN_V3_VIEWPORT_SCREENSHOT_REQUESTED",FRAME,str(PNG),flush=True)
    if REQUESTED and FRAME>=175 and PNG.is_file() and PNG.stat().st_size>=30000:
        print("SSLM_TERRAIN_V3_VIEWPORT_FILE_SAVED",PNG.stat().st_size,flush=True)
        stop()
    elif FRAME>=420:
        print("SSLM_TERRAIN_V3_VIEWPORT_SCREENSHOT_NOT_READY",FRAME,flush=True)
        stop()

if not hasattr(unreal,"register_slate_post_tick_callback"):
    raise RuntimeError("UE Python未暴露Slate post-tick回调，不能采用同步阻塞sleep伪造帧等待")
HANDLE=unreal.register_slate_post_tick_callback(tick)
print("SSLM_TERRAIN_V3_VIEWPORT_TICKED_SCHEDULED map="+MAP+" no_assets_modified=true",flush=True)
