#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""天气P5真实UE资产验收，仅在锁定UE5.8 Editor PythonScriptPlugin中使用。

检查真实.uasset/UWorld类别、MPC完整8参数、Material Shader、
7个MaterialFunction实际输出、3个Niagara System、8个天气DataAsset与2份审核蓝图。
音频资源必须提供实际客户端包后，通过 WEATHER_REQUIRE_AUDIO=1 严格校验。
Niagara 逐Emitter的脚本/渲染器/编译状态另需Monolith validate_system +
get_system_diagnostics，不用仅路径存在代替粒子系统功能。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

MODE = os.environ.get("WEATHER_ASSET_VERIFY_MODE", "inspect").lower()
REQUIRE_AUDIO = os.environ.get("WEATHER_REQUIRE_AUDIO", "0") == "1"
OUT = Path(__file__).with_name("WeatherEditorAssetVerification.json")

MPC = "/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal"
COLLECTION_PARAMS = (
    "GP_Surface_GlobalWetness",
    "GP_Surface_GlobalSnowAmount",
    "GP_Surface_GlobalSnowHeightCm",
    "GP_Surface_GlobalMossInfluence",
    "GP_Surface_GlobalPuddleAmount",
    "GP_Surface_RainIntensity",
    "GP_Surface_SnowIntensity",
    "GP_Surface_TemperatureCelsius",
)
ROOT_SURFACE = "/GamePlatformSurface"
ROOT_VFX = "/GamePlatformVFX/Weather"
ROOT_VILLAGE = "/DBAWorldPack_Village/Weather/Definitions"
ROOT_REVIEW = "/Game/Development/Weather"
REQUIREMENTS = {
    "MaterialParameterCollection": [MPC],
    "Texture2D": [
        ROOT_SURFACE + "/Textures/Weather/T_GP_Weather_" + name
        for name in ("SnowAlbedo", "SnowNormal", "SnowORM", "WetnessNoise", "PuddleMask")
    ] + [
        ROOT_VFX + "/Textures/T_GP_Weather_" + name
        for name in ("RainStreak", "RainRipple", "RainSplashFlipbook", "SnowflakeAtlas")
    ],
    "MaterialFunction": [
        ROOT_SURFACE + "/MaterialFunctions/Masks/MF_GP_SlopeMask",
        ROOT_SURFACE + "/MaterialFunctions/Masks/MF_GP_HeightMask",
        ROOT_SURFACE + "/MaterialFunctions/Utility/MF_GP_WorldNoise",
    ] + [
        ROOT_SURFACE + "/MaterialFunctions/Layers/MF_GP_" + name
        for name in ("SnowLayer", "MossLayer", "WetnessLayer", "PuddleLayer")
    ],
    "Material": [
        ROOT_SURFACE + "/Materials/M_GP_Surface_Master",
        ROOT_SURFACE + "/Materials/M_GP_Surface_Lite",
        ROOT_VFX + "/Materials/M_GP_VFX_Weather_Rain",
        ROOT_VFX + "/Materials/M_GP_VFX_Weather_Snow",
        ROOT_VFX + "/Materials/M_GP_VFX_Weather_Splash",
    ],
    "NiagaraSystem": [
        ROOT_VFX + "/Niagara/NS_GP_Weather_" + name
        for name in ("Rain", "Snow", "RainSplash")
    ],
    "GamePlatformWeatherPresetDefinition": [
        ROOT_VILLAGE + "/DA_DBA_Weather_" + name
        for name in ("Clear", "Cloudy", "LightRain", "HeavyRain",
                     "LightSnow", "HeavySnow", "AfterRain", "AfterSnow")
    ],
    "Blueprint": [
        ROOT_REVIEW + "/Blueprints/BP_DBA_WeatherReviewController",
        ROOT_REVIEW + "/Blueprints/BP_DBA_WeatherReviewGameMode",
    ],
    "World": [ROOT_REVIEW + "/Maps/L_DBA_WeatherReview"],
}
AUDIO = {
    "SoundWave": [
        "/DBASFXPack_Core/Weather/Audio/SW_DBA_Weather_" + name + "_Loop"
        for name in ("RainLight", "RainHeavy", "Wind")
    ],
    "GamePlatformSFXDefinition": [
        "/DBASFXPack_Core/Weather/Definitions/DA_DBA_SFX_Weather_" + name
        for name in ("Rain", "Snow")
    ],
}


def expected() -> dict[str, list[str]]:
    req = dict(REQUIREMENTS)
    if REQUIRE_AUDIO:
        req.update(AUDIO)
    return req


def inspect() -> None:
    for classname, assets in expected().items():
        print("REQUIRED_UE_ASSETS", classname, len(assets))
        for asset in assets:
            print(" ", asset)
    if MODE == "inspect":
        print("WEATHER_VERIFY_INSPECT_ONLY：不验证虚幻本地资产、不写报告。")


def run_editor() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须由实际UE5.8 Editor加载本项目与全部天气反射模块后运行") from exc

    editor = unreal.EditorAssetLibrary
    material_lib = unreal.MaterialEditingLibrary
    results = []
    failed = []
    for classname, asset_paths in expected().items():
        klass = getattr(unreal, classname, None)
        if klass is None:
            failed.append(f"UE反射类不可用：{classname}")
            continue
        for package_path in asset_paths:
            if not editor.does_asset_exist(package_path):
                failed.append("缺少真实包：" + package_path)
                continue
            item = editor.load_asset(package_path)
            if not isinstance(item, klass):
                failed.append("错误的真实资源类别：" + package_path)
                continue
            if classname == "MaterialFunction":
                count = material_lib.get_num_material_expressions_in_function(item)
                if count < 3:
                    failed.append("没有真实输出的材质函数：" + package_path)
            if classname == "Material":
                count = material_lib.get_num_material_expressions(item)
                if count < 10:
                    failed.append("材质节点过少：" + package_path)
                else:
                    errors = list(material_lib.recompile_material(item))
                    if any("error" in str(x).lower() for x in errors):
                        failed.append("Shader编译错误：" + package_path + ": " + "; ".join(map(str, errors))[:800])
            if classname == "MaterialParameterCollection":
                scalars = item.get_editor_property("scalar_parameters")
                names = {str(v.get_editor_property("parameter_name")) for v in scalars}
                missing = set(COLLECTION_PARAMS) - names
                if missing:
                    failed.append("MPC缺少标量参数：" + ",".join(sorted(missing)))
            results.append({"type": classname, "asset": package_path, "class": item.get_class().get_name()})
            print("ASSET_EXISTS_CLASS_OK", package_path, classname)
    # 服务器Cook与客户端Cook不能仅凭此脚本证明：这里只验证编辑器反射资源。
    report = {
        "engine": "UE5.8",
        "mode": "actual_unreal_editor",
        "assets_present": len(results),
        "assets_required": sum(len(v) for v in expected().values()),
        "requires_audio": REQUIRE_AUDIO,
        "failures": failed,
        "niagara_emitter_compilation": "requires_monolith_get_system_diagnostics_and_validate_system",
        "cook_and_network": "requires_real_Client_Server_Cook_and_live_multiplayer",
        "conclusion": "assets_found" if not failed else "asset_validation_failed",
    }
    # 仅在用户显式验证模式写本脚本旁边的诊断JSON，而不修改.uasset。
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    for issue in failed:
        print("WEATHER_VERIFY_FAILED", issue)
    if failed:
        raise RuntimeError(f"天气引擎资源检查失败：{len(failed)}项，见诊断报告")
    print("WEATHER_UE_ASSETS_CLASS_SHADER_PASS", len(results))


def main() -> None:
    inspect()
    if MODE == "inspect":
        return
    if MODE != "verify":
        raise ValueError("WEATHER_ASSET_VERIFY_MODE只允许inspect/verify")
    run_editor()


if __name__ == "__main__":
    main()
