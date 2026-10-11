#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""使用真实UE5.8编辑器制作平台天气VFX Definition数据资产（雨、雪）。

正式Niagara系统及其Emitters必须已由Monolith/UE编辑器创建、Shader编译、保存。
本脚本不会创建空系统，也不会在雨雪Niagara不存在时制造假Definition。
默认WEATHER_VFX_DEFINITION_MODE=inspect，只有apply才创建两个GamePlatformVFXAttachedDefinition。
"""
from __future__ import annotations
import os

MODE = os.environ.get("WEATHER_VFX_DEFINITION_MODE", "inspect").strip().lower()
DEST = "/GamePlatformVFX/Weather/Definitions"
CASES = (
    ("DA_GP_VFX_Weather_Rain", "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Rain",
     "platform.weather.rain@1"),
    ("DA_GP_VFX_Weather_Snow", "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow",
     "platform.weather.snow@1"),
)


def main() -> None:
    for name, system, logical_id in CASES:
        print("VFX_DEFINITION_TARGET", DEST + "/" + name, system, logical_id)
    if MODE == "inspect":
        print("WEATHER_VFX_DEFINITION_INSPECT_ONLY：未创建任何uasset")
        return
    if MODE != "apply":
        raise ValueError("WEATHER_VFX_DEFINITION_MODE只能是inspect或apply")
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须在UE5.8编辑器中执行，不允许普通Python生成虚假Definition") from exc

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    klass = unreal.GamePlatformVFXAttachedDefinition
    for name, system, _ in CASES:
        if not assets.does_asset_exist(system):
            raise FileNotFoundError("未找到真实Niagara System：" + system)
        obj = assets.load_asset(system)
        if not isinstance(obj, unreal.NiagaraSystem):
            raise RuntimeError(f"Niagara真实资产类型不合法：{system}")
        if assets.does_asset_exist(DEST + "/" + name):
            # 允许同类同系统的中断续作；绝不覆盖引用到其他Niagara的Definition。
            existing = assets.load_asset(DEST + "/" + name)
            if not isinstance(existing, klass):
                raise TypeError("已有同名资源不是平台天气Definition：" + name)
            current = existing.get_editor_property("niagara_system")
            if current and current.get_path_name() != obj.get_path_name():
                raise RuntimeError("现存定义引用其他Niagara，拒绝覆盖：" + name)

    if not assets.does_directory_exist(DEST):
        assets.make_directory(DEST)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", klass)

    for name, system, logical_id in CASES:
        path = DEST + "/" + name
        d = (assets.load_asset(path) if assets.does_asset_exist(path)
             else tools.create_asset(name, DEST, klass, factory))
        if not isinstance(d, klass):
            raise RuntimeError("无法创建真实VFX定义：" + name)
        namespace_and_name, generation = logical_id.rsplit("@", 1)
        namespace, simple_name = namespace_and_name.rsplit(".", 1)
        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", namespace)
        ident.set_editor_property("name", simple_name)
        ident.set_editor_property("logical_version", int(generation))
        d.set_editor_property("logical_id", ident)
        revision = unreal.GamePlatformDataVersion()
        # C++定义默认SchemaVersion/ContentRevision=1，不在临时Python结构实例上修改。
        d.set_editor_property("data_version", revision)
        d.set_editor_property("niagara_system", assets.load_asset(system))
        d.set_editor_property("auto_destroy", False)
        d.set_editor_property("allow_pooling", True)
        d.set_editor_property("enable_scalability", True)

        # EditDefaultsOnly字段在UE5.8临时结构体上不能set_editor_property；
        # 用UStruct关键字构造器一次性初始化，再写入真正的DataAsset默认对象。
        def scalar_rule(param_name: str, maximum: float):
            return unreal.GamePlatformVFXParameterRule(
                name=unreal.Name(param_name),
                type=unreal.GamePlatformVFXParameterType.FLOAT,
                required=True,
                min_value=0.0,
                max_value=maximum,
            )

        schema = unreal.GamePlatformVFXParameterSchema(
            max_override_count=16,
            rules=[
                scalar_rule("User.WeatherIntensity", 1.0),
                scalar_rule("User.WeatherNearSpawnRate", 1600.0),
                scalar_rule("User.WeatherFarSpawnRate", 1000.0),
            ],
        )
        d.set_editor_property("parameter_schema", schema)

        if not assets.save_loaded_asset(d, only_if_is_dirty=False):
            raise RuntimeError("VFX Definition保存失败：" + name)
        check = assets.load_asset(DEST + "/" + name)
        if not isinstance(check, klass):
            raise RuntimeError("VFX Definition回读类型异常：" + name)
        print("UE_WEATHER_VFX_DEFINITION_SAVED", check.get_path_name())


if __name__ == "__main__":
    main()
