#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 地形函数组合源码与三层边界的只读回归。

测试运行不加载UE，不新建.uasset，不修改地图/材质；真实节点连线、Shader与GPU仍需UE验收。
"""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TERRAIN = ROOT / "Tools/Unreal/Terrain/AuthorTerrainProductionMaterial.py"
FUNCTION = ROOT / "Tools/Unreal/Surface/AuthorTerrainPBRLayerBlendUE58.py"
WORLD = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap")
PLATFORM = ROOT / ("Game/Plugins/GamePlatform/Presentation/"
                   "GamePlatformSurface/Content/MaterialFunctions")
CLIENT = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
                 "DBAPresentationPack_Core/Content/Materials/Terrain")


def load_module(path: Path, module_name: str):
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError("无法加载待验证的作者模块：" + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def ensure(value, reason):
    if not value:
        raise AssertionError("SSLM地形函数组合静态校验失败：" + reason)


def run():
    material_source = TERRAIN.read_text(encoding="utf-8")
    platform_source = FUNCTION.read_text(encoding="utf-8")
    import ast
    ast.parse(material_source, filename=str(TERRAIN))
    ast.parse(platform_source, filename=str(FUNCTION))

    original = os.environ.get("TERRAIN_FUNCTION_INTEGRATION")
    try:
        os.environ["TERRAIN_FUNCTION_INTEGRATION"] = "0"
        legacy = load_module(TERRAIN, "terrain_legacy_contract")
        ensure(legacy.MASTER.endswith("/M_DBA_Terrain_Production10"),
               "原正式PBR资产身份被改变")
        ensure(legacy.INSTANCE.endswith("/MI_DBA_Terrain_Production10_Review"),
               "原审核材质实例身份被改变")
        os.environ["TERRAIN_FUNCTION_INTEGRATION"] = "1"
        candidate = load_module(TERRAIN, "terrain_function_candidate")
        ensure(candidate.MASTER.endswith("/M_DBA_Terrain_Production10_FunctionIntegrated"),
               "函数化候选必须有独立母材质身份")
        ensure(candidate.INSTANCE.endswith("/MI_DBA_Terrain_Production10_FunctionIntegrated_Review"),
               "函数化候选必须有独立MI身份")
    finally:
        if original is None:
            os.environ.pop("TERRAIN_FUNCTION_INTEGRATION", None)
        else:
            os.environ["TERRAIN_FUNCTION_INTEGRATION"] = original

    ensure(len(candidate.LAYERS) == 10 and len(set(candidate.LAYERS)) == 10,
           "十层项目资产身份不唯一")
    ensure(legacy.LAYERS == candidate.LAYERS,
           "候选地形图层与原正式资源身份不一致")
    ensure(candidate.validate_manifest() == legacy.validate_manifest(),
           "候选不能变动扫描纹理来源合同")

    author = load_module(FUNCTION, "platform_terrain_pbr_layer_author")
    spec = candidate.SHARED_FUNCTIONS
    ensure(len(spec) == 6, "须接入宏观/距离/坡度/海拔/生态/四通道覆盖六类共享函数")
    shared_inputs = spec["PBRLayer"][1]
    shared_outputs = spec["PBRLayer"][2]
    ensure(shared_inputs == set(author.INPUTS), "PBR函数输入引脚合同不一致")
    ensure(shared_outputs == set(author.OUTPUTS), "PBR函数输出引脚合同不一致")

    for key, (relative, inputs, outputs) in spec.items():
        path = PLATFORM / (relative + ".uasset")
        if key != "PBRLayer":
            ensure(path.is_file(), "被引用平台函数不存在：" + str(path))
        ensure(relative.startswith(("Landscape/", "Layers/")),
               "函数调用路径必须只依赖平台Surface领域")
        ensure(len(inputs) > 0 and len(outputs) > 0,
               "平台材质函数没有完整输入输出定义：" + key)

    for sentinel in (
        "MaterialExpressionMaterialFunctionCall",
        'pbr_layer(col,n,rough,ao',
        'call_shared("MacroColor"',
        'call_shared("DistanceDetail"',
        'call_shared("SlopeLayers"',
        'call_shared("AltitudeLayers"',
        'call_shared("BiomeMask"',
        'call_shared("PBRLayer"',
        "TERRAIN_FUNCTION_INTEGRATION",
        "TerrainMossAmount",
        "GP_Surface_GlobalSnowAmount",
        "GP_Surface_GlobalWetness",
        "TerrainEnableHeightBlend",
        "SSM_WRAP_WORLD_GROUP_SETTINGS",
        "MP_AMBIENT_OCCLUSION",
    ):
        ensure(sentinel in material_source, "真实函数化材质作者缺少合同：" + sentinel)

    for sentinel in (
        "MaterialExpressionFunctionInput",
        "MaterialExpressionFunctionOutput",
        "MaterialExpressionLinearInterpolate",
        "MaterialExpressionNormalize",
        "MaterialExpressionSaturate",
        "create_material_expression_in_function",
        "update_material_function",
        "save_loaded_asset",
    ):
        ensure(sentinel in platform_source, "平台PBR函数缺少实际作者节点：" + sentinel)
    ensure("Game/Plugins/DivineBeasts" not in platform_source,
           "平台函数作者不应引用项目层资源路径")

    # 未批准迁移前，正式共享地图不得硬依赖新的纯客户端候选母材质。
    data = WORLD.read_bytes()
    ensure(b"M_DBA_Village_LandscapeBridge" in data,
           "新手村权威地图必须继续使用现有中性材质")
    ensure(b"M_DBA_Terrain_Production10_FunctionIntegrated" not in data,
           "未经验收的高精候选不得进入共享权威地图")
    ensure(CLIENT.is_dir(), "既有项目客户端表现内容包不存在")

    print("TERRAIN_FUNCTION_INTEGRATION_STATIC_PASS",
          "existing_platform_functions=5",
          "new_generic_PBR_function=1",
          "PBR_properties=4",
          "Landscape_layers=10",
          "candidate_isolated=true")


if __name__ == "__main__":
    run()
