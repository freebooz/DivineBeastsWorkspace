#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8独立渲染进程入口：CliffMacro完整4K Mip实际SceneCapture复审。

仅载入已经完成独立三轮重启回读的L_DBA_CliffRock01MacroReview；
禁止接管用户已打开的其它关卡；具体GPU纹理驻留、拍照和照明恢复由
CaptureCliffRock01MacroFullMipGPU.py负责。此入口不创建新UE资产或改源图。
"""
from __future__ import annotations

import runpy
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
REVIEW=ROOT/"Tools/Unreal/Terrain/CaptureCliffRock01MacroFullMipGPU.py"


def main()->None:
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
        "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE宏观GPU复审不属于正式神兽联盟工作区")
    world=unreal.EditorLevelLibrary.get_editor_world()
    current=world.get_path_name() if isinstance(world,unreal.World) else ""
    print("CLIFF_MACRO_GPU_FULLMIP_INITIAL_MAP",current)
    if MAP not in current:
        del world
        if not unreal.EditorLevelLibrary.load_level(MAP):
            raise RuntimeError("不能载入独立CliffMacro审核地图")
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise RuntimeError("最终GPU画面所属关卡不正确")
    del world
    runpy.run_path(str(REVIEW),run_name="__main__")
    print("CLIFF_MACRO_FULLMIP_GPU_PROBE_COMPLETED")


if __name__=="__main__":
    main()
