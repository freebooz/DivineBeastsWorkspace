#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实UE5.8雪层材质、Niagara和项目审核资产加载门禁（只读）。

此脚本通过UE编辑器或Monolith run_python执行，检查引擎类别、材质图节点与
项目材质实例父类、独立审核关卡存在、Niagara System真实加载及天气Definition绑定。
不以文件大小代替UE加载，不启动性能Profile或冒充真人画面评估。
"""

from __future__ import annotations

import os


def run() -> None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须由UE5.8 Editor运行真实资产验收") from exc
    assets = unreal.EditorAssetLibrary
    base = "/GamePlatformSurface"
    vfx = "/GamePlatformVFX/Weather"
    project = "/DBAPresentationPack_Core/Materials/Snow"
    required = {
        base + "/Textures/Snow/T_GP_Snow_MicroHeight": unreal.Texture2D,
        base + "/Textures/Snow/T_GP_Snow_CoverageNoise": unreal.Texture2D,
        vfx + "/Textures/T_GP_Snow_GroundPuff": unreal.Texture2D,
        base + "/Materials/M_GP_SnowCover_Detailed": unreal.Material,
        vfx + "/Materials/M_GP_VFX_SnowGroundPuff": unreal.Material,
        project + "/MI_DBA_Snow_Detailed": unreal.MaterialInstanceConstant,
        vfx + "/Niagara/NS_GP_Weather_Snow_Detailed": unreal.NiagaraSystem,
        "/Game/Development/Snow/Maps/L_DBA_SnowReview": unreal.World,
    }
    found = {}
    for package, expected_class in required.items():
        if not assets.does_asset_exist(package):
            raise FileNotFoundError("真实积雪UE资产缺失：" + package)
        obj = assets.load_asset(package)
        if not isinstance(obj, expected_class):
            raise TypeError("UE资产类别与要求不一致：" + package)
        found[package] = obj
        print("SNOW_UE_ASSET_LOAD_OK", package, obj.get_class().get_name())
    material_lib = unreal.MaterialEditingLibrary
    master = found[base + "/Materials/M_GP_SnowCover_Detailed"]
    puff = found[vfx + "/Materials/M_GP_VFX_SnowGroundPuff"]
    instance = found[project + "/MI_DBA_Snow_Detailed"]
    if material_lib.get_num_material_expressions(master) < 50:
        raise RuntimeError("写实雪母材质节点数不足")
    if material_lib.get_num_material_expressions(puff) < 6:
        raise RuntimeError("贴地雪粉材质节点缺失")
    if not puff.get_editor_property("used_with_niagara_sprites"):
        raise RuntimeError("贴地雪粉材质缺少Niagara Sprite用途标记")
    if instance.get_editor_property("parent") != master:
        raise RuntimeError("神兽联盟材质实例未继承平台雪母材质")
    print("SNOW_UE_MATERIAL_CONTRACT_OK", material_lib.get_num_material_expressions(master),
          material_lib.get_num_material_expressions(puff))

    definition_path = vfx + "/Definitions/DA_GP_VFX_Weather_Snow"
    if not assets.does_asset_exist(definition_path):
        raise FileNotFoundError("平台雪Definition缺失：" + definition_path)
    definition = assets.load_asset(definition_path)
    if not isinstance(definition, unreal.GamePlatformVFXAttachedDefinition):
        raise TypeError("平台雪Definition类型不匹配")
    bound = definition.get_editor_property("niagara_system")
    if bound != found[vfx + "/Niagara/NS_GP_Weather_Snow_Detailed"]:
        raise RuntimeError("正式天气仍未绑定增强版雪Niagara")
    print("SNOW_UE_PRODUCTION_DEFINITION_BOUND", definition_path)
    print("SNOW_UE_ASSET_VALIDATION_PASS", len(required), "additional=1")


if __name__ == "__main__":
    run()
