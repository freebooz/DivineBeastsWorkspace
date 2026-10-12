#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8正常GUI编辑器雪苔独立审核：先预热Shader与场景RHI再捕获三态。

仅作用于本轮启动的UE编辑器进程，不保存/覆盖地图、不干扰其他编辑器。
采用Slate PostTick异步计时，不在编辑器启动时立即捕获黑色RenderTarget。
Python回调在本进程内由builtins持有，完成/失败均注销并请求本次Editor退出。
"""
from __future__ import annotations
import builtins
import runpy
import os
import time
import traceback
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview"
CAPTURE=ROOT/"Tools/Unreal/Surface/CaptureMossSnowGPUReview.py"
START_WAIT_SECONDS=7.0
SHADER_WARM_SECONDS=43.0

def main():
    import unreal
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("禁止在其他游戏工程执行雪苔GUI材质审查")
    if os.getenv("MOSS_SNOW_GPU_MODE")!="apply":
        raise RuntimeError("GPU截图必须显式设置MOSS_SNOW_GPU_MODE=apply")
    if not hasattr(unreal,"register_slate_post_tick_callback"):
        raise RuntimeError("UE5.8当前环境无Slate逐帧回调，不能提供真实GUI预热")
    if getattr(builtins,"__moss_gui_capture_handle",None) is not None:
        raise RuntimeError("当前进程已有雪苔审查任务，拒绝重复注册")
    if not CAPTURE.is_file():
        raise FileNotFoundError("缺少已有可审计GPU截图脚本："+str(CAPTURE))
    start=time.monotonic()
    state={"handle":None,"loaded_at":None,"done":False,"last":start,"ticks":0}

    def finish():
        handle=state.get("handle")
        if handle is not None:
            try:
                unreal.unregister_slate_post_tick_callback(handle)
            finally:
                state["handle"]=None
                builtins.__moss_gui_capture_handle=None
        world=unreal.EditorLevelLibrary.get_editor_world()
        if isinstance(world,unreal.World):
            unreal.SystemLibrary.execute_console_command(world,"QUIT")
            print("MOSS_SNOW_GUI_OWN_EDITOR_QUIT_REQUESTED",flush=True)

    def tick(delta_seconds):
        if state["done"]:return
        state["ticks"]+=1
        now=time.monotonic()
        if now-state["last"]<.16:return
        state["last"]=now
        try:
            if state["loaded_at"] is None:
                if now-start<START_WAIT_SECONDS:return
                editor=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                if not editor or not editor.load_level(MAP):
                    raise RuntimeError("GUI完整引擎无法加载独立雪苔审核图")
                state["loaded_at"]=time.monotonic()
                print("MOSS_SNOW_GUI_REVIEW_WORLD_LOADED",MAP,flush=True)
                return
            elapsed=now-state["loaded_at"]
            if elapsed<SHADER_WARM_SECONDS:return
            state["done"]=True
            print("MOSS_SNOW_GUI_SHADER_WARM_COMPLETED",
                "seconds",round(elapsed,1),"ticks",state["ticks"],flush=True)
            runpy.run_path(str(CAPTURE),run_name="__main__")
            print("MOSS_SNOW_GUI_UE_CAPTURE_SCRIPT_SUCCEEDED",flush=True)
            finish()
        except Exception:
            traceback.print_exc()
            state["done"]=True
            print("MOSS_SNOW_GUI_UE_CAPTURE_SCRIPT_FAILED",flush=True)
            finish()

    state["handle"]=unreal.register_slate_post_tick_callback(tick)
    builtins.__moss_gui_capture_handle=(state["handle"],tick,state)
    print("MOSS_SNOW_GUI_DELAYED_CAPTURE_REGISTERED",flush=True)

if __name__=="__main__":
    main()
