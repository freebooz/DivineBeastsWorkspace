#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3地形配色独立审核地图的真实RHI六态取图入口。

复用现有 CaptureTerrainProductionGPUReview.py (平台MPC/SceneCapture2D规则)，
只将地图和材质实例替换为新建的V3项目层审查候选；不复制捕获算法、不修改正式Village。
默认inspect只读；TERRAIN_COLOR_V3_GPU_MODE=apply时仅写Saved证据，
绝不修改、覆盖、保存原有Landscape或项目地图资产。
GPU截图必须经过真实像素完整性门禁及人工审核，截图不能直接保证60/120FPS。
"""
from __future__ import annotations

import os
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.getenv("TERRAIN_COLOR_V3_GPU_MODE", "inspect").lower().strip()
SCRIPT = ROOT / "Tools/Unreal/Terrain/CaptureTerrainProductionGPUReview.py"
MAP = "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review"
MI = ("/DBAWorldPack_Village/Development/Terrain/Production10/"
      "MI_DBA_Terrain_Production10_V3_Review")


def main() -> None:
    if MODE not in {"inspect", "apply", "verify"}:
        raise ValueError("SSLM V3 GPU取图仅允许inspect、apply、verify")
    if not SCRIPT.is_file():
        raise RuntimeError("已有正式RHI采集器不存在，不允许伪造测试帧")
    if MODE == "inspect":
        print("SSLM_TERRAIN_COLOR_V3_GPU_PLAN", MAP, MI,
              "D3D12_real_RHI_required=true, approval_pending=true")
        return
    # 复用已存在的真实RHI执行机制；独立输出，不触碰旧GPU证据目录。
    execution = runpy.run_path(str(SCRIPT), run_name="SSLM_existing_terrain_gpu_capture")
    execute = execution["execute"]
    execute.__globals__.update({
        "MODE": MODE,
        "MAP": MAP,
        "MI": MI,
        "OUTPUT_NAME": "TerrainColorV3_GPU_PreloadedFixed_20261018",
    })
    execute()
    print("SSLM_TERRAIN_COLOR_V3_GPU_WRAPPER_DONE", MODE,
          "no_formal_map_mutation=true", flush=True)


if __name__ == "__main__":
    main()
