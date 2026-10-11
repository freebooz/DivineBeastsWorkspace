#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01低振幅4组真实GPU复测：独立编辑器入口。

所有改动仅在隔离QA进程中临时执行，保留正式Village和已有岩石材质资源。
"""
from pathlib import Path
import runpy

ROOT=Path(__file__).resolve().parents[3]
CAPTURE=ROOT/"Tools/Unreal/Terrain/CaptureCliffRock01LowWarpSameSceneGPU.py"

def main():
    import unreal
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("未加载神兽联盟正式UE5.8工程")
    if not CAPTURE.is_file():
        raise FileNotFoundError("低振幅D3D12场景截图脚本缺失")
    runpy.run_path(str(CAPTURE),run_name="__main__")
    print("CLIFF_LOWWARP_INDEPENDENT_D3D12_CAPTURE_FINISHED")

if __name__=="__main__":
    main()
