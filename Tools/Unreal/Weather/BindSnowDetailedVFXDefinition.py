#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""将已通过真实Niagara编译的增强雪系统接入平台既有天气Definition。

唯一变更：同一 GamePlatformVFXAttachedDefinition 的 niagara_system 指针；
逻辑资源身份 platform.weather.snow@1、参数Schema、异步目录和服务端天气事实保持不变。
原有NS_GP_Weather_Snow完整保留，用于无代码回滚；拒绝创建占位Definition。
必须在正确UE5.8编辑器进程内运行，且独立运行Niagara检验确认三Emitter为valid。
默认SNOW_DETAILED_BIND_MODE=inspect；apply允许切换到增量新系统。
"""

from __future__ import annotations

import os

MODE = os.getenv("SNOW_DETAILED_BIND_MODE", "inspect").lower().strip()
DEFINITION = "/GamePlatformVFX/Weather/Definitions/DA_GP_VFX_Weather_Snow"
NEW_SYSTEM = "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow_Detailed"
ORIGINAL_SYSTEM = "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow"


def execute() -> None:
    print("SNOW_DEFINITION_REBIND_PLAN", DEFINITION, NEW_SYSTEM)
    if MODE == "inspect":
        return
    if MODE != "apply":
        raise ValueError("SNOW_DETAILED_BIND_MODE仅允许inspect/apply")
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须通过真实UE5.8编辑器加载Definition") from exc
    if unreal.Paths.get_project_file_path().replace("\\", "/").split("/")[-1] != "DivineBeastsArena.uproject":
        raise RuntimeError("拒绝修改非神兽联盟编辑器的天气Definition")
    asset_lib = unreal.EditorAssetLibrary
    for package in (DEFINITION, ORIGINAL_SYSTEM, NEW_SYSTEM):
        if not asset_lib.does_asset_exist(package):
            raise FileNotFoundError("雨雪权威表现资源缺失：" + package)
    definition = asset_lib.load_asset(DEFINITION)
    if not isinstance(definition, unreal.GamePlatformVFXAttachedDefinition):
        raise TypeError("当前Definition不是平台UGamePlatformVFXAttachedDefinition")
    detailed = asset_lib.load_asset(NEW_SYSTEM)
    original = asset_lib.load_asset(ORIGINAL_SYSTEM)
    if not isinstance(detailed, unreal.NiagaraSystem) or not isinstance(original, unreal.NiagaraSystem):
        raise TypeError("Niagara System类型不匹配")

    existing = definition.get_editor_property("niagara_system")
    # 强制要求原引用仍为正式旧版本或已经指向新版本，拒绝绕过第三方内容变更。
    current_path = existing.get_path_name() if existing else ""
    if current_path not in (original.get_path_name(), detailed.get_path_name()):
        raise RuntimeError("Definition现有系统非受控旧/新版本，拒绝覆盖：" + current_path)
    if current_path == detailed.get_path_name():
        print("SNOW_DEFINITION_ALREADY_DETAILED", DEFINITION)
        return

    definition.set_editor_property("niagara_system", detailed)
    if not asset_lib.save_loaded_asset(definition, only_if_is_dirty=False):
        raise RuntimeError("正式天气Definition无法保存")
    loaded = asset_lib.load_asset(DEFINITION)
    bound = loaded.get_editor_property("niagara_system")
    if not bound or bound.get_path_name() != detailed.get_path_name():
        raise RuntimeError("正式Definition回读的Niagara不匹配")
    print("SNOW_DEFINITION_REBOUND_AND_SAVED", DEFINITION, bound.get_path_name())
    print("SNOW_ROLLBACK_SYSTEM_PRESERVED", ORIGINAL_SYSTEM)


if __name__ == "__main__":
    execute()
