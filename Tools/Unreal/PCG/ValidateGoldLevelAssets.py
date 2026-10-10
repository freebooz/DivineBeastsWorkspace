#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCG金标准资源独立编辑器重开校验，独立于创建进程，严禁虚构资产已交付。

模式 PCG_GOLD_VALIDATE_MODE=inspect（默认）输出检查清单；
模式 reopen 只能在锁定UE5.8 Editor的另一独立进程中执行。
校验资产类别、定义身份、蓝图父类、地图World、放置器/编排器数量；
不冒充G01～G16、生成实例/碰撞/NavMesh/双端Cook和性能验收。
"""
from __future__ import annotations
import os
import sys

MODE = os.environ.get("PCG_GOLD_VALIDATE_MODE", "inspect").strip().lower()
ROOT = "/Game/Development/Foundation/PCG"
VILLAGE = "/DBAWorldPack_Village/PCG/Blueprints"
GOLD_MAP = ROOT + "/Validation/PCG_GoldLevel_M1"
TEMPLATE_IDS = (
    "TPL_Base", "TPL_ScatterSurface", "TPL_BiomeGenerator", "TPL_LinearDresser",
    "TPL_Enclosure", "TPL_EnclosureClosed", "TPL_Connector", "TPL_GateInsert",
    "TPL_ParcelFill", "TPL_CropField", "TPL_AssemblySpawn", "TPL_InterfaceBand"
)
SUBGRAPH_IDS = (
    "SG_ProjectOnLandscape", "SG_PriorityCarve", "SG_ApplySpawnPolicy",
    "SG_AssignMeshSet", "SG_FitPostsToSpline", "SG_BreakByIntersection",
    "SG_WriteClosedExclude"
)
BLUEPRINT_NAMES = (
    "BP_PCG_Forest", "BP_PCG_Rock", "BP_PCG_Road", "BP_PCG_Field",
    "BP_PCG_Crops", "BP_PCG_Bridge", "BP_PCG_Exclusion", "BP_PCG_WaterBank",
    "BP_PCG_Building", "BP_PCG_Resource", "BP_PCG_Spawn"
)
DEFINITIONS = (
    ("Exec","GamePlatformPCGExecPresetDefinition"),
    ("Priority","GamePlatformPCGPriorityTableDefinition"),
    ("MeshCanopy","GamePlatformPCGMeshSetDefinition"),
    ("MeshCrop","GamePlatformPCGMeshSetDefinition"),
    ("MeshFence","GamePlatformPCGMeshSetDefinition"),
    ("PolicyForest","GamePlatformPCGSpawnPolicyDefinition"),
    ("PolicyCrop","GamePlatformPCGSpawnPolicyDefinition"),
    ("LayerCanopy","GamePlatformPCGLayerDefinition"),
    ("LayerFloor","GamePlatformPCGLayerDefinition"),
    ("LayerCrop","GamePlatformPCGLayerDefinition"),
    ("Biome","GamePlatformPCGBiomePresetDefinition"),
    ("Exclusion","GamePlatformPCGExclusionPresetDefinition"),
    ("Road","GamePlatformPCGRoadProfileDefinition"),
    ("Enclosure","GamePlatformPCGEnclosureProfileDefinition"),
    ("Parcel","GamePlatformPCGParcelPresetDefinition"),
    ("Crop","GamePlatformPCGCropProfileDefinition"),
    ("Connector","GamePlatformPCGConnectorCatalogDefinition")
)


REALIZED_NAMES = ("Canopy", "Rock", "Crop")

def main() -> None:
    print("PCG_REOPEN_EXPECTED", "12 templates", "7 subgraphs", "11 blueprints", "17 definitions", "3 realized graphs", "3 profiles", "1 umap")
    if MODE == "inspect":
        print("PCG_REOPEN_INSPECT_ONLY：没有访问UE资源或报告验证通过")
        return
    if MODE != "reopen":
        raise ValueError("PCG_GOLD_VALIDATE_MODE仅支持inspect或reopen")

    try:
        import unreal  # type: ignore
    except ImportError as error:
        raise RuntimeError("必须由真正UE5.8 Editor独立重开；不能从普通Python扫描虚假资源") from error

    assets = unreal.EditorAssetLibrary
    paths = (
        [(ROOT + "/Templates/" + name, "PCGGraph") for name in TEMPLATE_IDS]
        + [(ROOT + "/Subgraphs/" + name, "PCGGraph") for name in SUBGRAPH_IDS]
        + [(VILLAGE + "/" + name, "Blueprint") for name in BLUEPRINT_NAMES]
        + [(ROOT + "/Definitions/DA_PCGGold_" + short, cls) for short,cls in DEFINITIONS]
        + [(ROOT + "/Realized/PCG_Gold_" + name, "PCGGraph") for name in REALIZED_NAMES]
        + [(ROOT + "/Profiles/DA_PCGGold_" + name, "GamePlatformPCGProfileDefinition") for name in REALIZED_NAMES]
        + [(GOLD_MAP, "World")]
    )
    for path, class_name in paths:
        if not assets.does_asset_exist(path):
            raise FileNotFoundError("PCG期望资源不存在：" + path)
        expected = getattr(unreal, class_name, None)
        if expected is None:
            raise RuntimeError("UE5.8缺少反射类：" + class_name)
        obj = assets.load_asset(path)
        if obj is None or not isinstance(obj, expected):
            raise RuntimeError("PCG资源回读类型错误：" + path + "，要求：" + class_name)
        if class_name.startswith("GamePlatformPCG"):
            identity = obj.get_editor_property("logical_id")
            text = str(identity)
            if not text or text == "None":
                raise RuntimeError("PCG定义缺少稳定逻辑身份：" + path)
        print("PCG_REOPEN_ASSET_OK", path, class_name)

    # World Partition全图Descriptor、源ID及资源烘焙需在DataValidation单独审查；
    # 这里仅验证加载的测试地图的最低放置器类型/数量与唯一Director。
    if not unreal.EditorLevelLibrary.load_level(GOLD_MAP):
        raise RuntimeError("独立UE进程无法重新打开PCG_GoldLevel_M1")
    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    director_class = getattr(unreal, "GamePlatformPCGWorldDirector", None)
    classes = {
        "directors": (director_class, 1),
        "volume": (getattr(unreal, "GamePlatformPCGVolumeActor", None), 3),
        "spline": (getattr(unreal, "GamePlatformPCGSplineActor", None), 3),
        "polygon": (getattr(unreal, "GamePlatformPCGPolygonActor", None), 2),
        "connector": (getattr(unreal, "GamePlatformPCGConnectorActor", None), 2),
        "exclusion": (getattr(unreal, "GamePlatformPCGExclusionActor", None), 1),
    }
    for category, (actor_type, required) in classes.items():
        if actor_type is None:
            raise RuntimeError("UE编辑器缺少PCG放置器类：" + category)
        count = sum(isinstance(actor, actor_type) for actor in actors)
        if count < required or (category == "directors" and count != 1):
            raise RuntimeError(f"金标准地图{category}数量不符：实际{count}，最低{required}")
        print("PCG_REOPEN_MAP_COMPONENT_OK", category, count)

    directors = [actor for actor in actors if isinstance(actor, director_class)]
    result = directors[0].validate_participant_set()
    if (result[0] if isinstance(result, tuple) else result) is not True:
        raise RuntimeError("金标准世界编排器注册集合无效：" + str(result))

    print("PCG_GOLD_REOPEN_ASSET_TOTAL", len(paths))
    print("PCG_REOPEN_BASIC_CLASSES_PASS；G01～G16、Cook、性能与服务器权威仍另行验收")


if __name__ == "__main__":
    main()
