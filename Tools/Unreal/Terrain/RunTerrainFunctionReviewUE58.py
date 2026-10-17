#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 地形函数化独立UE编辑器回读、三质量实例、独立审核地图执行器。

先独立反射验证PBR层函数与8个真实调用，再增量创建三档MI并验证已有独立审核地图。
地图必须已在单独编辑器进程经AuthorTerrainFunctionReviewMapUE58.py(copy)复制保存；
禁止本进程在复制UWorld后立即调用LoadLevel，避免UE5.8 World Memory Leaks崩溃。
不编译项目C++、不改变共享地图/已有十层资源、不进入正式客户端/服务器。
环境 TERRAIN_FUNCTION_REVIEW_MODE=apply|verify（默认inspect）。
"""
from __future__ import annotations

import os
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.environ.get("TERRAIN_FUNCTION_REVIEW_MODE", "inspect").strip().lower()


def stage(script: str, var: str, mode: str) -> None:
    os.environ[var] = mode
    try:
        runpy.run_path(str(ROOT / "Tools/Unreal/Terrain" / script), run_name="__main__")
    finally:
        os.environ.pop(var, None)


def run() -> None:
    if MODE not in {"inspect", "apply", "verify"}:
        raise ValueError("SSLM地形函数化审查只允许inspect/apply/verify")
    print("TERRAIN_FUNCTION_REVIEW_EXECUTION_PLAN", MODE)
    if MODE == "inspect":
        return
    # 函数创建工具内部使用独立变量；此处始终Verify，不会覆盖平台现有MF和母材质。
    os.environ["TERRAIN_FUNCTION_COMPOSITION_MODE"] = "verify"
    try:
        runpy.run_path(
            str(ROOT / "Tools/Unreal/Terrain/RunTerrainFunctionCompositionUE58.py"),
            run_name="__main__")
    finally:
        os.environ.pop("TERRAIN_FUNCTION_COMPOSITION_MODE", None)

    os.environ["TERRAIN_FUNCTION_INTEGRATION"] = "1"
    try:
        stage("AuthorTerrainProductionQualityProfiles.py",
              "TERRAIN_QUALITY_PROFILES_MODE", MODE)
        stage("AuthorTerrainFunctionReviewMapUE58.py",
              "TERRAIN_FUNCTION_REVIEW_MAP_MODE", MODE)
    finally:
        os.environ.pop("TERRAIN_FUNCTION_INTEGRATION", None)
    print("TERRAIN_FUNCTION_REVIEW_NATIVE_VERIFIED",
          "mode=" + MODE,
          "quality=High,Medium,Low",
          "landscape=1",
          "map_isolated=true")


if __name__ == "__main__":
    run()
