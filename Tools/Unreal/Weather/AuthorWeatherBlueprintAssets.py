#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟天气P0数据资产与开发审核蓝图生成器（仅UE5.8 Editor可执行）。

资产范围：
  Server-safe数据定义8个 -> DBAWorldPack_Village/Weather/Definitions
  可派生的真实审查蓝图2个（GameMode/Actor） -> /Game/Development/Weather/Blueprints

默认WEATHER_BLUEPRINT_AUTHOR_MODE=inspect，只输出清单。
apply之前必须编辑器已加载DBAWorldsRuntime和GamePlatformWeatherRuntime。
所有生成资源必须执行UE编译、保存和重新加载确认。
拒绝任何目标重名；不会修改正式新手村地图、替换游戏入口或触发网络权威。
"""
from __future__ import annotations
import json
import os
from pathlib import Path

MODE = os.environ.get("WEATHER_BLUEPRINT_AUTHOR_MODE", "inspect").strip().lower()
DEFINITION_DIR = "/DBAWorldPack_Village/Weather/Definitions"
REVIEW_DIR = "/Game/Development/Weather/Blueprints"
REVIEW_BLUEPRINT = REVIEW_DIR + "/BP_DBA_WeatherReviewController"
REVIEW_GAMEMODE_BLUEPRINT = REVIEW_DIR + "/BP_DBA_WeatherReviewGameMode"

# 具体数值只用于服务器安全的天气预设，不硬引用客户端Niagara/音效/纹理。
# 字段 = (天气类型,降雨强度,降雪强度,湿润,积雪,积水,雾,风,摄氏温度,过渡秒数)
PRESETS = (
    ("Clear", "Clear",         0,    0,   0,   0,   0,    0,   0.10, 20,  8),
    ("Cloudy", "Cloudy",       0,    0,   0,   0,   0,  .18,   0.16, 15,  8),
    ("LightRain", "LightRain", .35,    0, .35,   0, .08,  .08,   0.20, 13, 10),
    ("HeavyRain", "HeavyRain", .92,    0, .92,   0, .55,  .24,   0.40, 11, 10),
    ("LightSnow", "LightSnow", 0,   .35,   0, .25,   0, .08,    .18, -2, 12),
    ("HeavySnow", "HeavySnow", 0,   .90,   0, .84,   0, .26,    .47, -8, 12),
    ("AfterRain", "Clear",     0,     0, .55,   0, .25,   0,    .15, 16, 40),
    ("AfterSnow", "Clear",     0,     0,   0, .70,   0,   0,    .15, -3, 40),
)
ENUM_VALUES = {
    "Clear": "CLEAR", "Cloudy": "CLOUDY",
    "LightRain": "LIGHT_RAIN", "HeavyRain": "HEAVY_RAIN",
    "LightSnow": "LIGHT_SNOW", "HeavySnow": "HEAVY_SNOW",
}

def plans() -> list[str]:
    return [f"{DEFINITION_DIR}/DA_DBA_Weather_{rec[0]}" for rec in PRESETS] + [
        REVIEW_BLUEPRINT, REVIEW_GAMEMODE_BLUEPRINT
    ]


def create_assets():
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("UE天气蓝图只能在加载真实UE5.8 Editor时创建，禁止普通Python写.uasset") from exc
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()

    parent_def = getattr(unreal, "GamePlatformWeatherPresetDefinition", None)
    parent_review = getattr(unreal, "DivineBeastsWeatherReviewController", None)
    parent_game_mode = getattr(unreal, "DivineBeastsWorldGameMode", None)
    if not parent_def or not parent_review or not parent_game_mode:
        raise RuntimeError("项目天气反射C++类型未加载，请先完成编辑器模块链接")

    for target in plans():
        if assets.does_asset_exist(target):
            raise FileExistsError(f"项目天气资产已存在，拒绝覆盖：{target}")
    for folder in (DEFINITION_DIR, REVIEW_DIR):
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)

    data_factory = unreal.DataAssetFactory()
    data_factory.set_editor_property("data_asset_class", parent_def)

    def enum(name: str):
        # 运行时使用UE反射枚举而非自己猜测Enum Python值，错误时明确失败。
        enum_type = getattr(unreal, "GamePlatformWeatherType", None)
        if enum_type is None:
            raise RuntimeError("缺少EGamePlatformWeatherType反射枚举")
        try:
            return getattr(enum_type, ENUM_VALUES[name])
        except AttributeError as exc:
            raise RuntimeError(f"UE天气枚举缺少{ENUM_VALUES[name]}") from exc

    def state_to_unreal(record: tuple):
        state = unreal.GamePlatformWeatherState()
        state.set_editor_property("type", enum(record[1]))
        for prop, val in zip(
            ("rain_intensity", "snow_intensity", "wetness", "snow_amount",
             "puddle_amount", "fog_intensity", "wind_intensity",
             "temperature_celsius"), record[2:10]
        ):
            state.set_editor_property(prop, float(val))
        return state

    for row in PRESETS:
        name = "DA_DBA_Weather_" + row[0]
        asset = tools.create_asset(name, DEFINITION_DIR, parent_def, data_factory)
        if asset is None or not isinstance(asset, parent_def):
            raise RuntimeError(f"没有创建真实的GamePlatformWeatherPresetDefinition：{name}")
        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", "db.weather")
        ident.set_editor_property("name", row[0].lower())
        ident.set_editor_property("logical_version", 1)
        asset.set_editor_property("logical_id", ident)
        version = unreal.GamePlatformDataVersion()
        version.set_editor_property("schema_version", 1)
        version.set_editor_property("content_revision", 1)
        asset.set_editor_property("data_version", version)

        entry = unreal.GamePlatformWeatherScheduleEntry()
        entry.set_editor_property("state", state_to_unreal(row))
        entry.set_editor_property("min_hold_seconds", 60.0)
        entry.set_editor_property("max_hold_seconds", 120.0)
        entry.set_editor_property("transition_seconds", float(row[10]))
        entry.set_editor_property("weight", 1)
        asset.set_editor_property("weather", entry)
        if not assets.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError(f"UE数据资产保存失败：{name}")
        # 首次完成至少验证实际类别与关键身份；完整Data定义合法性放在集中自动化阶段。
        reloaded = assets.load_asset(f"{DEFINITION_DIR}/{name}")
        if reloaded is None or not isinstance(reloaded, parent_def):
            raise RuntimeError(f"天气预设回读类别错误：{name}")
        print("UE_WEATHER_PRESET_SAVED", reloaded.get_path_name())

    # 创建两个真正的项目蓝图：Review GameMode只组合现有权威天气机制，
    # Review Actor只供地图审查按钮/人工蓝图事件调用，不在前端或正式竞技中自动执行。
    bp_editor = getattr(unreal, "BlueprintEditorLibrary", None)
    compiler = getattr(bp_editor, "compile_blueprint", None)
    if compiler is None:
        kismet = getattr(unreal, "KismetEditorUtilities", None)
        compiler = getattr(kismet, "compile_blueprint", None)
    if compiler is None:
        raise RuntimeError("UE5.8蓝图编译API不可用；拒绝创建无法校验的蓝图")

    for name, parent in (
        ("BP_DBA_WeatherReviewController", parent_review),
        ("BP_DBA_WeatherReviewGameMode", parent_game_mode),
    ):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = tools.create_asset(name, REVIEW_DIR, unreal.Blueprint, factory)
        if not blueprint or not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError("真实审核蓝图创建失败：" + name)
        compiler(blueprint)
        if not assets.save_loaded_asset(blueprint, only_if_is_dirty=False):
            raise RuntimeError("蓝图编译后保存失败：" + name)
        loaded = assets.load_asset(REVIEW_DIR + "/" + name)
        if not isinstance(loaded, unreal.Blueprint):
            raise RuntimeError("蓝图保存后回读失败：" + name)
        print("UE_WEATHER_BLUEPRINT_SAVED", loaded.get_path_name())

    # 天气预设通过项目GameMode的InitialWeatherPresetId由GamePlatformData世界租约加载。
    # 审核GameMode默认保持晴天，用户可在蓝图类默认值中选择db.weather.lightrain@1等ID。


def main() -> None:
    for path in plans():
        print("WEATHER_ASSET_PLAN", path)
    if MODE == "inspect":
        print("WEATHER_BLUEPRINT_INSPECT_ONLY：未写入.uasset")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_BLUEPRINT_AUTHOR_MODE只允许inspect/apply")
    create_assets()


if __name__ == "__main__":
    main()
