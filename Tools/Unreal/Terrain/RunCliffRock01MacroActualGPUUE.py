#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在单独的UE5.8渲染进程中运行宏观悬崖20m GPU审核（不会接管用户Editor）。

目标审核World已由三个完全独立的UE进程创建、绑定并重新加载验证。
本脚本仅在特定CliffMacroReview World已加载后触发2次SceneCapture2D；
不同的用户角色预览世界或未初始化真实RHI均拒绝。绝不覆盖先前旧图或源World。
初始地图优先通过UnrealEditor命令行独立地图参数载入；
如果编辑器启动没加载指定地图则在当前Python进程中只加载已保存地图
（这里不复制UWorld，无前一轮duplicate+load的内存持有问题）。
截图脚本自带finally恢复审核灯光，不能代替真人签收。
"""
from __future__ import annotations

import runpy
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
CAPTURE=ROOT/"Tools/Unreal/Terrain/CaptureCliffRock01MacroGPU.py"


def main()->None:
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
        "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("只允许在正式DivineBeastsArena工程检查宏观Cliff")
    world=unreal.EditorLevelLibrary.get_editor_world()
    path=world.get_path_name() if isinstance(world,unreal.World) else ""
    print("CLIFF_MACRO_GPU_START_WORLD",path)
    if MAP not in path:
        del world
        if not unreal.EditorLevelLibrary.load_level(MAP):
            raise RuntimeError("隔离GPU编辑器进程无法加载已保存CliffMacroReview真实地图")
    target=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(target,unreal.World) or MAP not in target.get_path_name():
        raise RuntimeError("当前已不是隔离审核地图，禁止更换用户正在编辑的关卡")
    del target
    required=(
        ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011/"
        "CliffRock01Macro_20m_BalancedLights_ActualGPU.png",
        ROOT/"Game/Saved/Validation/TerrainGaea/CliffRock01Macro20261011/"
        "CliffRock01Macro_20m_SoftLights_ActualGPU.png"
    )
    if any(path.is_file() for path in required):
        raise FileExistsError("新的GPU截图目标已存在，不能覆盖之前审核证据")
    if not CAPTURE.is_file():
        raise FileNotFoundError("真正UE5.8 SceneCapture2D操作脚本不存在")
    print("CLIFF_MACRO_GPU_NATIVE_SCENECAPTURE_START",MAP)
    runpy.run_path(str(CAPTURE),run_name="__main__")
    for path in required:
        if not path.is_file() or path.stat().st_size<60000:
            raise RuntimeError("真实RHI没有生成完整SceneCapture2D PNG："+str(path))
        print("CLIFF_MACRO_GPU_REAL_PNG_CREATED",path,"bytes",path.stat().st_size)
    print("CLIFF_MACRO_GPU_REVIEW_CAPTURE_FINISHED_TWO_ACTUAL_UE_FRAMES")


if __name__=="__main__":
    main()
