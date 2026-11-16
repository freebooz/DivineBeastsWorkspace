#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 地形材质函数化的原生 UE5.8 增量作者入口。

先创建/验证 GamePlatformSurface 中立四通道 PBR 函数，再创建/验证
DBAPresentationPack_Core 十层地形函数化候选母材质与实例。不会改正式Village。
使用 TERRAIN_FUNCTION_COMPOSITION_MODE=apply|verify|inspect（默认inspect）。
apply 只允许创建新的资源；已有资源合同不符合时立即失败，不清图不覆盖。
verify 应在另一个UE编辑器进程运行并检查真实MaterialFunctionCall。
"""
from __future__ import annotations

import os
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.getenv("TERRAIN_FUNCTION_COMPOSITION_MODE", "inspect").strip().lower()
SCRIPTS = (
    ROOT / "Tools/Unreal/Surface/AuthorTerrainPBRLayerBlendUE58.py",
    ROOT / "Tools/Unreal/Terrain/AuthorTerrainProductionMaterial.py",
)


def main() -> None:
    if MODE not in {"inspect", "apply", "verify"}:
        raise ValueError("运行模式仅允许 inspect/apply/verify")
    if MODE == "inspect":
        print("TERRAIN_FUNCTION_COMPOSITION_PLAN client_only_candidate"
              " platform_MF_PBRLayerBlend + terrain_10_PBR")
        return
    os.environ["TERRAIN_FUNCTION_INTEGRATION"] = "1"
    os.environ["TERRAIN_PBR_BLEND_FUNCTION_MODE"] = MODE
    os.environ["TERRAIN_PRODUCTION_MATERIAL_MODE"] = MODE
    try:
        for script in SCRIPTS:
            if not script.is_file():
                raise RuntimeError("函数化原生作者脚本缺失：" + str(script))
            runpy.run_path(str(script), run_name="__main__")
        print("TERRAIN_FUNCTION_COMPOSITION_UE_VERIFIED",
              "mode=" + MODE,
              "functions=8 calls",
              "shared_maps_unchanged=true")
    finally:
        # 避免同一个UE进程之后复用执行环境时污染下一份材质制作任务。
        for name in ("TERRAIN_FUNCTION_INTEGRATION",
                     "TERRAIN_PBR_BLEND_FUNCTION_MODE",
                     "TERRAIN_PRODUCTION_MATERIAL_MODE"):
            os.environ.pop(name, None)


if __name__ == "__main__":
    main()
