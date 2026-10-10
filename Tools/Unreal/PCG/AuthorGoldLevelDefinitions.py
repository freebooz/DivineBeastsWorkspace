#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCG Gold Level（PCG金标准关卡）Definition（数据定义）真实资产创作工具。

默认 PCG_GOLD_DEFINITION_MODE=inspect 仅输出清单；apply 只能由UE5.8 Editor执行。
不覆盖任何已保存的PCG图、Definition或项目资源，不通过文件系统模拟.uasset。
通过GamePlatformDefinition类型/RequiredDefinitions建立跨资产依赖关系；
引擎基础Mesh只用作功能验收，不是桃花村正式美术资源。
"""
from __future__ import annotations

import os

MODE = os.environ.get("PCG_GOLD_DEFINITION_MODE", "inspect").lower().strip()
ROOT = "/Game/Development/Foundation/PCG/Definitions"
MESHES = {
    "MeshCanopy": "/Engine/BasicShapes/Cylinder.Cylinder",
    "MeshCrop": "/Engine/BasicShapes/Sphere.Sphere",
    "MeshFence": "/Engine/BasicShapes/Cube.Cube",
}
ORDER = (
    "Exec", "Priority", "MeshCanopy", "MeshCrop", "MeshFence",
    "PolicyForest", "PolicyCrop", "LayerCanopy", "LayerFloor", "LayerCrop",
    "Biome", "Exclusion", "Road", "Enclosure", "Parcel", "Crop", "Connector",
)
TYPES = {
    "Exec": "GamePlatformPCGExecPresetDefinition",
    "Priority": "GamePlatformPCGPriorityTableDefinition",
    "MeshCanopy": "GamePlatformPCGMeshSetDefinition",
    "MeshCrop": "GamePlatformPCGMeshSetDefinition",
    "MeshFence": "GamePlatformPCGMeshSetDefinition",
    "PolicyForest": "GamePlatformPCGSpawnPolicyDefinition",
    "PolicyCrop": "GamePlatformPCGSpawnPolicyDefinition",
    "LayerCanopy": "GamePlatformPCGLayerDefinition",
    "LayerFloor": "GamePlatformPCGLayerDefinition",
    "LayerCrop": "GamePlatformPCGLayerDefinition",
    "Biome": "GamePlatformPCGBiomePresetDefinition",
    "Exclusion": "GamePlatformPCGExclusionPresetDefinition",
    "Road": "GamePlatformPCGRoadProfileDefinition",
    "Enclosure": "GamePlatformPCGEnclosureProfileDefinition",
    "Parcel": "GamePlatformPCGParcelPresetDefinition",
    "Crop": "GamePlatformPCGCropProfileDefinition",
    "Connector": "GamePlatformPCGConnectorCatalogDefinition",
}


def asset_path(short_name: str) -> str:
    return ROOT + "/DA_PCGGold_" + short_name


def identity(short_name: str) -> str:
    return "foundation.pcg_gold_" + short_name.lower() + "@1"


def inspect() -> None:
    print("PCG_GOLD_DEFINITION_ASSETS", len(ORDER))
    for name in ORDER:
        print("PLAN_DEFINITION", TYPES[name], asset_path(name), identity(name))


def main() -> None:
    inspect()
    if MODE == "inspect":
        print("PCG_GOLD_DEFINITION_INSPECT_ONLY：未创建任何.uasset")
        return
    if MODE != "apply":
        raise ValueError("PCG_GOLD_DEFINITION_MODE必须为inspect或apply")

    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只有真实UE5.8编辑器可写PCG DataAsset；普通Python严禁生成伪资源") from exc

    library = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    loaded_meshes = {}
    for short, path in MESHES.items():
        mesh = library.load_asset(path)
        if mesh is None or not isinstance(mesh, unreal.StaticMesh):
            raise FileNotFoundError("PCG Gold测试需要真实UE基础静态网格：" + path)
        loaded_meshes[short] = mesh

    runtime_types = {}
    for name, class_name in TYPES.items():
        cls = getattr(unreal, class_name, None)
        if cls is None:
            raise RuntimeError("平台PCG反射类型尚未由新版编辑器编译加载：" + class_name)
        runtime_types[name] = cls

    for name in ORDER:
        if library.does_asset_exist(asset_path(name)):
            raise FileExistsError("PCG Definition已有正式UE资产，禁止覆盖：" + asset_path(name))
    if not library.does_directory_exist(ROOT):
        library.make_directory(ROOT)

    def as_primary_asset_id(short):
        # 原始类型约定在GamePlatformData（平台数据）底层定义，不能使用另一个资产ID命名空间。
        logical = identity(short)
        ctor = getattr(unreal.PrimaryAssetId, "from_string", None)
        if callable(ctor):
            candidate = ctor("GamePlatformDefinition:" + logical)
            if candidate is not None:
                return candidate
        # UE5.8反射结构支持属性赋值；若引擎API变更则直接失败关闭，不省略依赖。
        kind = unreal.PrimaryAssetType()
        kind.set_editor_property("name", "GamePlatformDefinition")
        result = unreal.PrimaryAssetId()
        result.set_editor_property("primary_asset_type", kind)
        result.set_editor_property("primary_asset_name", logical)
        return result


    # 在创建第一份DataAsset前验证完整主资产ID构造链，避免已经写盘7个Definition后才遇到UE反射兼容错误。
    preflight_ids = {name: as_primary_asset_id(name) for name in ORDER}
    if len({str(value) for value in preflight_ids.values()}) != len(preflight_ids):
        raise RuntimeError("PCG Definition身份构造结果重复，拒绝开始真实保存。")

    def new_asset(short):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", runtime_types[short])
        result = tools.create_asset("DA_PCGGold_" + short, ROOT, runtime_types[short], factory)
        if result is None or not isinstance(result, runtime_types[short]):
            raise RuntimeError("UE资产工具未能创建正确的PCG Definition类型：" + short)
        logical = unreal.GamePlatformId()
        logical.set_editor_property("namespace", "foundation")
        logical.set_editor_property("name", "pcg_gold_" + short.lower())
        logical.set_editor_property("logical_version", 1)
        version = unreal.GamePlatformDataVersion()
        version.set_editor_property("schema_version", 1)
        version.set_editor_property("content_revision", 1)
        result.set_editor_property("logical_id", logical)
        result.set_editor_property("data_version", version)
        return result

    def require(asset, short, *dependencies):
        ids = [as_primary_asset_id(dep) for dep in dependencies]
        if len({str(value) for value in ids}) != len(ids):
            raise ValueError("PCG依赖身份重复：" + short)
        asset.set_editor_property("required_definitions", ids)

    def prop(asset, name, value):
        asset.set_editor_property(name, value)

    for short in ORDER:
        definition = new_asset(short)
        if short in MESHES:
            entry = unreal.GamePlatformPCGMeshSetEntry()
            prop(entry, "mesh", loaded_meshes[short])
            prop(entry, "weight", 1.0)
            prop(entry, "b_static_collision", short == "MeshFence")
            prop(definition, "entries", [entry])
        elif short == "PolicyForest":
            prop(definition, "density", 0.45)
            prop(definition, "self_prune_distance_cm", 130.0)
        elif short == "PolicyCrop":
            prop(definition, "density", 0.75)
            prop(definition, "self_prune_distance_cm", 30.0)
        elif short.startswith("Layer"):
            domain = {
                "LayerCanopy": ("Canopy", "MeshCanopy", "PolicyForest"),
                "LayerFloor": ("GroundCover", "MeshCanopy", "PolicyForest"),
                "LayerCrop": ("Crop", "MeshCrop", "PolicyCrop"),
            }
            label, mesh, policy = domain[short]
            prop(definition, "layer_name", label)
            prop(definition, "mesh_set_id", as_primary_asset_id(mesh))
            prop(definition, "spawn_policy_id", as_primary_asset_id(policy))
            require(definition, short, mesh, policy)
        elif short == "Biome":
            prop(definition, "biome_id", "GoldBiome")
            prop(definition, "layer_ids", [
                as_primary_asset_id("LayerCanopy"), as_primary_asset_id("LayerFloor"),
                as_primary_asset_id("LayerCrop")
            ])
            require(definition, short, "LayerCanopy", "LayerFloor", "LayerCrop")
        elif short == "Exclusion":
            prop(definition, "source_id", "ManualLock")
            prop(definition, "strength", 1.0)
        elif short == "Enclosure":
            prop(definition, "post_mesh_set_id", as_primary_asset_id("MeshFence"))
            prop(definition, "span_mesh_set_id", as_primary_asset_id("MeshFence"))
            require(definition, short, "MeshFence")
        elif short == "Parcel":
            prop(definition, "edge_enclosure_profile_id", as_primary_asset_id("Enclosure"))
            require(definition, short, "Enclosure")
        elif short == "Crop":
            prop(definition, "seasonal_mesh_set_ids", [as_primary_asset_id("MeshCrop")])
            require(definition, short, "MeshCrop")
        elif short == "Connector":
            for item_name in ("Gate", "Bridge"):
                entry = unreal.GamePlatformPCGConnectorCatalogEntry()
                entry.set_editor_property("item_id", "Gold" + item_name)
                entry.set_editor_property("content_definition_id", as_primary_asset_id("MeshFence"))
                if item_name == "Bridge":
                    entry.set_editor_property("type", unreal.GamePlatformPCGConnectorType.BRIDGE)
                else:
                    entry.set_editor_property("type", unreal.GamePlatformPCGConnectorType.GATE)
                existing = definition.get_editor_property("entries")
                prop(definition, "entries", list(existing) + [entry])
            require(definition, short, "MeshFence")

        if not library.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("UE5.8未能真实保存PCG定义：" + short)
        reloaded = library.load_asset(asset_path(short))
        if reloaded is None or not isinstance(reloaded, runtime_types[short]):
            raise RuntimeError("PCG定义保存后回读类别错误：" + asset_path(short))
        print("UE_PCG_GOLD_DEFINITION_SAVED", asset_path(short), identity(short))
    print("PCG_GOLD_DEFINITIONS_SAVED_TOTAL", len(ORDER))


if __name__ == "__main__":
    main()
