#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCG Gold Level（PCG金标准）真正带Spawner的Graph Instance与Profile创作工具。

默认 inspect 只输出清单；PCG_GOLD_REALIZE_MODE=apply 时需官方UE5.8 Editor + 已编译
GamePlatformPCGEditor（平台PCG编辑器模块）。只对Forest/Rock/Crop三条成熟输入链创建实例。
所有MeshSet来自先行Gold Definitions；任何ID、租约、模板、碰撞配置不一致时拒绝保存。
不执行PCG、不创造静态碰撞权威、不覆盖已有图/其他用户资产。
"""
from __future__ import annotations
import os

MODE = os.environ.get("PCG_GOLD_REALIZE_MODE", "inspect").lower().strip()
ROOT = "/Game/Development/Foundation/PCG"
DEFINITIONS = ROOT + "/Definitions"
REALIZED = ROOT + "/Realized"
PROFILES = ROOT + "/Profiles"
DESCRIPTORS = (
    # 配置后缀、Foundation模板、MeshSet定义、项目域输出用途
    ("Canopy", "TPL_ScatterSurface", "MeshCanopy", "COSMETIC"),
    ("Rock", "TPL_ScatterSurface", "MeshCanopy", "COSMETIC"),
    ("Crop", "TPL_CropField", "MeshCrop", "COSMETIC"),
)


def id_string(name: str) -> str:
    return "foundation.pcg_gold_" + name.lower() + "@1"


def actual_asset_path(category: str, name: str) -> str:
    return ROOT + "/" + category + "/" + name


def main() -> None:
    for label, template, mesh, usage in DESCRIPTORS:
        print("PCG_REALIZED_PLAN", label, template, mesh, usage,
              actual_asset_path("Realized", "PCG_Gold_" + label),
              actual_asset_path("Profiles", "DA_PCGGold_" + label))
    if MODE == "inspect":
        print("PCG_GOLD_REALIZE_INSPECT_ONLY：不创建Graph或Profile资产")
        return
    if MODE != "apply":
        raise ValueError("PCG_GOLD_REALIZE_MODE仅支持inspect或apply")

    try:
        import unreal  # type: ignore
    except ImportError as error:
        raise RuntimeError("UE5.8真实资产只能在Unreal Editor创建，不允许普通Python生成伪uasset") from error

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    cls = getattr(unreal, "GamePlatformPCGProfileDefinition", None)
    graph_cls = getattr(unreal, "PCGGraph", None)
    mesh_cls = getattr(unreal, "GamePlatformPCGMeshSetDefinition", None)
    editor_cls = getattr(unreal, "GamePlatformPCGEditorLibrary", None)
    maker = getattr(editor_cls, "create_development_realized_graph_asset", None) if editor_cls else None
    if cls is None or graph_cls is None or mesh_cls is None or not callable(maker):
        raise RuntimeError("UE5.8缺少新编译的PCG Profile/Graph/MeshSet/编辑器真实生成接口")

    # 防止半成品覆盖正式资源：对3个Profile与3个Graph先逐一做全部目标占用预检。
    all_paths = [
        actual_asset_path("Realized", "PCG_Gold_" + record[0]) for record in DESCRIPTORS
    ] + [
        actual_asset_path("Profiles", "DA_PCGGold_" + record[0]) for record in DESCRIPTORS
    ]
    for path in all_paths:
        if assets.does_asset_exist(path):
            raise FileExistsError("PCG真实图或配置已存在，禁止覆盖：" + path)

    loaded = {}
    for label, template_name, mesh_name, usage in DESCRIPTORS:
        template_path = actual_asset_path("Templates", template_name)
        mesh_path = actual_asset_path("Definitions", "DA_PCGGold_" + mesh_name)
        template = assets.load_asset(template_path)
        mesh = assets.load_asset(mesh_path)
        if not isinstance(template, graph_cls) or not isinstance(mesh, mesh_cls):
            raise FileNotFoundError("实际Foundation模板/网格集合定义尚未生成：" + template_path + ", " + mesh_path)
        loaded[label] = (template, mesh)


    configure = getattr(editor_cls, "configure_gold_development_profile", None)
    finalize = getattr(editor_cls, "finalize_gold_development_profile", None)
    if not callable(configure) or not callable(finalize):
        raise RuntimeError("GamePlatformPCGEditor缺少Gold Profile强类型初始化或最终图绑定入口")

    # 实际WeightedSpawner只消费已按PCGGeneration租约预加载的网格，不会在生成线程同步加载。
    for shape in ("/Engine/BasicShapes/Cylinder.Cylinder", "/Engine/BasicShapes/Sphere.Sphere"):
        preloaded = assets.load_asset(shape)
        if not isinstance(preloaded, unreal.StaticMesh):
            raise RuntimeError("真实静态网格预加载失败：" + shape)
    for folder in (REALIZED, PROFILES):
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)

    for label, template_name, mesh_name, usage in DESCRIPTORS:
        template, mesh = loaded[label]
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        profile_path = actual_asset_path("Profiles", "DA_PCGGold_" + label)
        profile = tools.create_asset("DA_PCGGold_" + label, PROFILES, cls, factory)
        if not isinstance(profile, cls):
            raise RuntimeError("UE未能创建Profile类型的DataAsset：" + profile_path)

        # 逻辑ID、Region、Definition Lease和EditDefaultsOnly字段由Editor C++强类型设置。
        configuration = configure(profile, label, template, mesh)
        configured = configuration[0] if isinstance(configuration, tuple) else configuration
        if configured is not True:
            raise RuntimeError("GoldLevel Profile初始化失败：" + label + " " + str(configuration))
        path = actual_asset_path("Realized", "PCG_Gold_" + label)
        raw = maker(path, profile, mesh)
        graph = next((x for x in raw if isinstance(x, graph_cls)), None) if isinstance(raw, tuple) else raw
        if not isinstance(graph, graph_cls):
            raise RuntimeError("UE没有产生批准的StaticMeshSpawner Graph：" + path + " " + str(raw))
        finalization = finalize(profile, graph)
        finished = finalization[0] if isinstance(finalization, tuple) else finalization
        if finished is not True:
            raise RuntimeError("GoldLevel实际Spawner图回写Profile失败：" + label + " " + str(finalization))
        if not assets.save_loaded_asset(profile, only_if_is_dirty=False):
            raise RuntimeError("PCG真实Profile不能保存：" + profile_path)
        reloaded = assets.load_asset(profile_path)
        if not isinstance(reloaded, cls):
            raise RuntimeError("PCG Profile保存后反射类型不一致：" + profile_path)
        print("PCG_GOLD_REALIZED_SAVED", path, profile_path, template_name, mesh_name)

    print("PCG_GOLD_REALIZED_COMPLETED", len(DESCRIPTORS))


if __name__ == "__main__":
    main()
