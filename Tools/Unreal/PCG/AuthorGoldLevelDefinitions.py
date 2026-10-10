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
    # UE5.8 Python反射将UPROPERTY bStaticCollision公开为static_collision而不是b_static_collision。
    # 先验证通用网格集合结构，防止创建部分DataAsset后才因字段名称异常中断。
    probe_entry = unreal.GamePlatformPCGMeshSetEntry()
    probe_entry.get_editor_property("static_collision")
    probe_entry.get_editor_property("mesh")
    probe_entry.get_editor_property("weight")
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

    def new_asset(short):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", runtime_types[short])
        result = tools.create_asset("DA_PCGGold_" + short, ROOT, runtime_types[short], factory)
        if result is None or not isinstance(result, runtime_types[short]):
            raise RuntimeError("UE资产工具未能创建正确的PCG Definition类型：" + short)
        # 具体默认值及依赖由Editor C++强类型配置，避免违反EditDefaultsOnly字段约束。
        return result

    configurator = getattr(unreal.GamePlatformPCGEditorLibrary, "configure_gold_development_definition", None)
    if not callable(configurator):
        raise RuntimeError("缺少最新版GamePlatformPCGEditor强类型Gold Definition初始化接口")

    # 全部17个Definition先初始化与C++强类型验证，再开始首次写盘；
    # 单项校验失败不会留下已保存的一半Gold测试资产。
    prepared = []
    for short in ORDER:
        definition = new_asset(short)
        result = configurator(definition, short)
        ok = result[0] if isinstance(result, tuple) else result
        if ok is not True:
            raise RuntimeError("GoldLevel定义初始化与RequiredDefinitions校验失败：" + short + " " + str(result))
        prepared.append((short, definition))

    for short, definition in prepared:
        if not library.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("UE5.8未能真实保存PCG定义：" + short)
        reloaded = library.load_asset(asset_path(short))
        if reloaded is None or not isinstance(reloaded, runtime_types[short]):
            raise RuntimeError("PCG定义保存后回读类别错误：" + asset_path(short))
        print("UE_PCG_GOLD_DEFINITION_SAVED", asset_path(short), identity(short))
    print("PCG_GOLD_DEFINITIONS_SAVED_TOTAL", len(ORDER))


if __name__ == "__main__":
    main()
