#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在一台真实UE5.8编辑器中增量创作完整积雪表面资源。

执行顺序：PNG源完整性预检 -> 真实Texture2D导入 -> 雪地PBR及雪粉材质 ->
DBA项目MI -> 可选独立审核地图。所有步骤均有资产验证、跳过已有资源及明确失败。
环境：
 SNOW_AUTHOR_PIPELINE_MODE=inspect|material|review|all（默认只读inspect）
 注意review必须在UE编辑器已创建项目MI后执行。
禁止覆盖项目正式地图、上传素材、伪造二进制或自动改变游戏天气权威。
"""

from __future__ import annotations

import os
import runpy
from pathlib import Path

HERE = Path(__file__).resolve().parent
PHASE = os.getenv("SNOW_AUTHOR_PIPELINE_MODE", "inspect").strip().lower()

STAGES = (
    ("ImportSnowDetailSourceArt.py", "SNOW_ASSET_IMPORT_MODE"),
    ("AuthorSnowDetailedAssets.py", "SNOW_MATERIAL_AUTHOR_MODE"),
)
REVIEW = ("AuthorSnowReviewMap.py", "SNOW_REVIEW_MAP_MODE")


def execute() -> None:
    if PHASE not in ("inspect", "material", "review", "all"):
        raise ValueError("SNOW_AUTHOR_PIPELINE_MODE只允许inspect/material/review/all")
    plan = (*STAGES, REVIEW) if PHASE == "all" else (REVIEW,) if PHASE == "review" else STAGES
    for name, _ in plan:
        print("SNOW_AUTHOR_STAGE_PLAN", name)
    if PHASE == "inspect":
        print("SNOW_AUTHOR_NO_ENGINE_ASSETS_MODIFIED")
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("资源必须在正确的UE5.8 Editor工程实例中创作") from exc

    if unreal.Paths.get_project_file_path().replace("\\", "/").split("/")[-1] != "DivineBeastsArena.uproject":
        raise RuntimeError("编辑器打开的不是正式神兽联盟工程，终止资产创作")
    print("SNOW_AUTHOR_ENGINE_PROJECT", unreal.Paths.get_project_file_path())
    for filename, setting in plan:
        before = os.environ.get(setting)
        os.environ[setting] = "apply"
        try:
            runpy.run_path(str(HERE / filename), run_name="__main__")
        finally:
            if before is None:
                os.environ.pop(setting, None)
            else:
                os.environ[setting] = before
        print("SNOW_AUTHOR_STAGE_DONE", filename)
    print("SNOW_AUTHOR_UE_STAGE_COMPLETED", PHASE)
    print("SNOW_FURTHER_VALIDATION_REQUIRED NiagaraRenderer/Shader/ClientServerCook/Manual")


if __name__ == "__main__":
    execute()
