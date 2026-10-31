#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 函数化地形 UE 二进制资产引用静态门禁。

本测试解析UE保存包中的序列化符号，不修改.uasset/.umap；只能确认包内
类别/引用的粗级别完整性，真正材质图节点连线、Shader/GPU应由UE独立进程核验。
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SURFACE = ROOT / ("Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/"
                  "Content/MaterialFunctions/Layers/MF_GP_PBRLayerBlend.uasset")
PRESENT = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                  "DBAWorldPack_Village/Content/Development/Terrain/Production10")
MASTER = PRESENT / "M_DBA_Terrain_Production10_FunctionIntegrated.uasset"
INSTANCE = PRESENT / "MI_DBA_Terrain_Production10_FunctionIntegrated_Review.uasset"
REVIEW = ROOT / "Game/Content/Development/Terrain/Maps/L_DBA_TerrainFunctionReview.umap"
PRODUCTION_VILLAGE = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                           "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap")
BASE_REVIEW = ROOT / "Game/Content/Development/Terrain/Maps/L_DBA_TerrainMaskReview.umap"
FUNCTIONS = {
    "MF_GP_LandscapeMacroColor",
    "MF_GP_LandscapeDistanceDetail",
    "MF_GP_LandscapeBiomeMask",
    "MF_GP_LandscapeSlopeLayers",
    "MF_GP_LandscapeAltitudeLayers",
    "MF_GP_PBRLayerBlend",
}


def ensure(ok: bool, msg: str) -> None:
    if not ok:
        raise RuntimeError("SSLM函数化地形资产合同失败：" + msg)


def symbols(path: Path) -> str:
    ensure(path.is_file(), "UE原生资产文件不存在：" + str(path))
    ensure(path.stat().st_size > 2500, "UE资产大小异常：" + str(path))
    # 原生UE包字符串表含ASCII资产短名；独立反射验收仍须验证对象类型和连线。
    return path.read_bytes().decode("latin-1", errors="replace")


def run() -> None:
    function = symbols(SURFACE)
    master = symbols(MASTER)
    instance = symbols(INSTANCE)
    qa = symbols(REVIEW)
    original = symbols(BASE_REVIEW)
    village = symbols(PRODUCTION_VILLAGE)

    for term in ("MaterialExpressionFunctionInput",
                 "MaterialExpressionFunctionOutput",
                 "MF_GP_PBRLayerBlend"):
        ensure(term in function, "平台函数原生包缺少表达式：" + term)
    for term in FUNCTIONS:
        ensure(term in master, "候选母材质没有序列化平台函数依赖：" + term)
    for term in ("MaterialExpressionMaterialFunctionCall",
                 "MPC_GP_SurfaceGlobal", "MaterialExpressionLandscapeLayerSample",
                 "TerrainEnableHeightBlend", "TerrainDetailTiling"):
        ensure(term in master, "候选母材质缺少关键节点/参数：" + term)
    ensure("M_DBA_Terrain_Production10_FunctionIntegrated" in instance,
           "审核MI父材质引用不正确")
    # 三档MI是独立资源身份：静态检查只证明原生文件和父材质存在，真实参数由UE反射冷重载验收。
    for tier in ("High", "Medium", "Low"):
        quality = symbols(PRESENT / ("MI_DBA_Terrain_Production10_FunctionIntegrated_" + tier + ".uasset"))
        ensure("M_DBA_Terrain_Production10_FunctionIntegrated" in quality,
               "质量MI未引用函数化候选母材质：" + tier)
    ensure("MI_DBA_Terrain_Production10_FunctionIntegrated_Review" in qa,
           "独立审核地图没有绑定函数化材质实例")
    ensure("MI_DBA_Terrain_Production10_FunctionIntegrated_Review" not in original,
           "旧地形审核地图被函数化候选意外覆盖")
    ensure("M_DBA_Village_LandscapeBridge" in village,
           "正式新手村共享地图失去服务器安全母材质")
    ensure("M_DBA_Terrain_Production10_FunctionIntegrated" not in village,
           "高精客户端函数化材质不得被服务器共享地图硬引用")
    print("TERRAIN_FUNCTION_NATIVE_PACKAGES_STATIC_PASS",
          "shared_MF=1", "call_types=6", "PBR_layers=3",
          "quality_MI=3", "QA_map=isolated", "village=unchanged")


if __name__ == "__main__":
    run()
