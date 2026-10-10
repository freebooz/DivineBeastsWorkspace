#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""天气P5按依赖顺序一次启动UE5.8 Editor制作真实资源。

用于：真正加载DivineBeastsArena.uproject、GamePlatformSurfaceEditor/WeatherRuntime、
PythonScriptPlugin以后，通过UnrealEditor-Cmd.exe -ExecutePythonScript调用本脚本。
前置：必须已由现有GamePlatformSurfaceCoreAssets Commandlet实际创建并验证MPC。
限制：绝不伪造.uasset/.umap，拒绝覆盖已有资产；失败保留现场等待人工修复，
不因PNG文件存在或脚本成功就声称真实Niagara、蓝图或MetaSound已交付。
为避免UE命令行过度启动，所有同一阶段Python作者工具在一个真实Editor进程运行。
"""

from __future__ import annotations

import os
import runpy
from pathlib import Path

HERE = Path(__file__).resolve().parent
PHASE = os.environ.get("WEATHER_AUTHOR_PHASE", "inspect").strip().lower()

PHASES = {
    "texture": [
        ("ImportWeatherSourceArt.py", "WEATHER_ASSET_IMPORT_MODE"),
    ],
    "surface": [
        ("AuthorWeatherSurfaceAssets.py", "WEATHER_SURFACE_AUTHOR_MODE"),
    ],
    "vfx-material": [
        ("AuthorWeatherVFXMaterials.py", "WEATHER_VFX_MATERIAL_MODE"),
    ],
    "blueprint": [
        ("AuthorWeatherBlueprintAssets.py", "WEATHER_BLUEPRINT_AUTHOR_MODE"),
    ],
    "review-map": [
        ("AuthorWeatherReviewMap.py", "WEATHER_REVIEW_MAP_MODE"),
    ],
    "vfx-definition": [
        ("AuthorWeatherVFXDefinitions.py", "WEATHER_VFX_DEFINITION_MODE"),
    ],
    "audio": [
        ("AuthorWeatherAudioAssets.py", "WEATHER_SFX_AUTHOR_MODE"),
    ],
}
PREPARE = ("texture", "surface", "vfx-material", "blueprint", "review-map")


def plans() -> list[tuple[str, str, str]]:
    if PHASE == "inspect":
        sequence = PREPARE
    elif PHASE == "prepare":
        sequence = PREPARE
    elif PHASE in PHASES:
        sequence = (PHASE,)
    else:
        raise ValueError("WEATHER_AUTHOR_PHASE仅支持inspect/prepare/texture/surface/vfx-material/blueprint/review-map/vfx-definition/audio")
    return [(step, name, mode) for step in sequence for name, mode in PHASES[step]]


def main() -> None:
    operations = plans()
    for step, file, _ in operations:
        print("WEATHER_AUTHOR_PLAN", step, file)
    if PHASE == "inspect":
        print("WEATHER_AUTHOR_INSPECT_ONLY：只描述任务，不写真实引擎资产。")
        return

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "必须在已装载GamePlatformWeather的UE5.8 Editor内运行；"
            "普通Python不能创作真实.uasset或.umap"
        ) from exc

    assets = unreal.EditorAssetLibrary
    mpc = "/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal"
    if not assets.does_asset_exist(mpc):
        raise FileNotFoundError(
            "Surface MPC前置命令尚未执行/验证：" + mpc + "；"
            "请先运行GamePlatformSurfaceCoreAssets命令及-ValidateOnly回读"
        )
    # 针对UE5.8真实资产体系：先检查脚本及明确的内置插件类/挂载点，
    # 不重新发明独立AssetManager，不修改任何World服务器权威。
    if not getattr(unreal, "GamePlatformWeatherPresetDefinition", None):
        raise RuntimeError("GamePlatformWeatherRuntime真实反射类型尚未加载；检查Editor DLL")
    if not getattr(unreal, "DivineBeastsWorldGameMode", None):
        raise RuntimeError("DBAWorldsRuntime真实GameMode反射类型尚未加载；检查Editor DLL")
    for step, script, mode_var in operations:
        path = HERE / script
        if not path.is_file():
            raise FileNotFoundError("天气作者工具脚本不存在：" + str(path))
        previous = os.environ.get(mode_var)
        os.environ[mode_var] = "apply"
        print("WEATHER_AUTHOR_UE_BEGIN", step, script)
        try:
            runpy.run_path(str(path), run_name="__main__")
        finally:
            if previous is None:
                os.environ.pop(mode_var, None)
            else:
                os.environ[mode_var] = previous
        print("WEATHER_AUTHOR_UE_SUCCESS", step, script)
    print("WEATHER_AUTHOR_UE_PHASE_COMPLETED", PHASE, "tasks", len(operations))
    print("UNREAL_ASSETS_REQUIRE_SEPARATE_VALIDATION: Niagara, shader errors, Cook, PIE and client/server packaging")


if __name__ == "__main__":
    main()
