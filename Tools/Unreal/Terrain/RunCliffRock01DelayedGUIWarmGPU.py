#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01正常GUI渲染编辑器的延迟GPU审核入口。

UE5.8正常UnrealEditor.exe + D3D12和RenderOffscreen独立测试，不使用Editor-Cmd立即截屏；
通过Slate PostTick回调，先打开独立保存的QA图，让实时Shader/Texture有>=45秒准备，
再调标准独立GPU SceneCapture2D并仅在此进程中退出。所有地图和正式Village不保存/修改。
注册回调在进程局部，成功或异常都注销，输出必须由D3D12真PNG像素质量门禁复核。
"""
from __future__ import annotations

import builtins
import os
import runpy
import time
import traceback
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
CAPTURE=ROOT/"Tools/Unreal/Terrain/CaptureCliffRock01LowWarpIsolatedGPU.py"
VARIANT=os.environ.get("CLIFF_ISOLATED_CAPTURE_VARIANT","")
WORLD={
 "baseline":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpBaselineReview",
 "lowWarp":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpVariantReview",
 "fullWarp":"/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview",
}
START_WAIT_SEC=7.0
PREVIEW_WARMUP_SEC=48.0

def main():
    import unreal
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("仅允许神兽联盟唯一项目的孤立图形审核")
    if VARIANT not in WORLD:
        raise ValueError("必须指定baseline、lowWarp或fullWarp")
    if os.environ.get("CLIFF_TEXTURE_STREAMING_MODE")!="on" or os.environ.get("CLIFF_CAPTURE_RUN_TAG")!="GUIWarm":
        raise RuntimeError("GUIWarm采用固定Streaming On与独立输出目录")
    if not CAPTURE.is_file():
        raise FileNotFoundError("标准独立SceneCapture2D脚本尚未创建")
    if not hasattr(unreal,"register_slate_post_tick_callback"):
        raise RuntimeError("UE5.8 Python缺少SlatePostTick回调，不可假称已预热")
    if getattr(builtins,"__cliff_guiwarm_handle",None) is not None:
        raise RuntimeError("同进程已经注册另一个Cliff审核，不允许重入")
    beginning=time.monotonic()
    state={"loaded":False,"captured":False,"last_tick":beginning,"ticks":0,"handle":None}
    def end():
        handle=state.get("handle")
        if handle is not None:
            try:unreal.unregister_slate_post_tick_callback(handle)
            finally:
                state["handle"]=None
                builtins.__cliff_guiwarm_handle=None
        world=unreal.EditorLevelLibrary.get_editor_world()
        if isinstance(world,unreal.World):
            unreal.SystemLibrary.execute_console_command(world,"QUIT")
            print("CLIFF_GUIWARM_OWN_EDITOR_QUIT_REQUESTED",flush=True)

    def tick(delta_sec):
        state["ticks"]+=1
        now=time.monotonic()
        if now-state["last_tick"]<.15:
            return
        state["last_tick"]=now
        try:
            if not state["loaded"] and now-beginning>=START_WAIT_SEC:
                previous=unreal.EditorLevelLibrary.get_editor_world()
                if not isinstance(previous,unreal.World) or WORLD[VARIANT] not in previous.get_path_name():
                    del previous
                    if not unreal.EditorLevelLibrary.load_level(WORLD[VARIANT]):
                        raise RuntimeError("UE GUI编辑器未能加载审核图："+WORLD[VARIANT])
                state["loaded"]=True
                state["loaded_at"]=time.monotonic()
                print("CLIFF_GUIWARM_D3D12_WORLD_LOADED",WORLD[VARIANT],flush=True)
                return
            if not state["loaded"]:
                return
            elapsed=now-state["loaded_at"]
            if elapsed>=PREVIEW_WARMUP_SEC and not state["captured"]:
                state["captured"]=True
                print("CLIFF_GUIWARM_REAL_UE_DELAY_COMPLETED",
                      "seconds",round(elapsed,2),"ticks",state["ticks"],
                      "variant",VARIANT,flush=True)
                runpy.run_path(str(CAPTURE),run_name="__main__")
                print("CLIFF_GUIWARM_D3D12_QA_CAPTURE_COMPLETED",VARIANT,flush=True)
                end()
        except Exception:
            traceback.print_exc()
            print("CLIFF_GUIWARM_D3D12_QA_FAILED",VARIANT,flush=True)
            state["captured"]=True
            end()

    h=unreal.register_slate_post_tick_callback(tick)
    if h is None:
        raise RuntimeError("Slate PostTick未返回有效回调Handle")
    state["handle"]=h
    # 使用本进程Python生命周期范围内builtins保留callback对象与DelegateHandle。
    builtins.__cliff_guiwarm_handle=(h,tick,state)
    print("CLIFF_GUIWARM_DELAYED_QA_REGISTERED",VARIANT,
          "worldWaitSec",START_WAIT_SEC,"warmupSec",PREVIEW_WARMUP_SEC,flush=True)


if __name__=="__main__":
    main()
