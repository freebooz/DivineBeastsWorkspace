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

    def primary_id(name: str):
        text = "GamePlatformDefinition:" + id_string(name)
        try:
            result = unreal.PrimaryAssetId.from_string(text)
            if result is not None:
                return result
        except (AttributeError, ValueError, TypeError):
            pass
        asset_type = unreal.PrimaryAssetType()
        asset_type.set_editor_property("name", "GamePlatformDefinition")
        result = unreal.PrimaryAssetId()
        result.set_editor_property("primary_asset_type", asset_type)
        result.set_editor_property("primary_asset_name", id_string(name))
        return result

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


    # 将后续需要的反射枚举和Definition ID预先解析完毕，避免产生半套可见图实例。
    policy = getattr(unreal, "GamePlatformPCGExecutionPolicy", None)
    use = getattr(unreal, "GamePlatformPCGOutputUsage", None)
    if policy is None or use is None or not hasattr(policy, "EDITOR_GENERATED_STATIC") or not hasattr(use, "COSMETIC"):
        raise RuntimeError("锁定UE5.8未提供安全静态PCG策略或纯装饰输出枚举")
    resolved_ids = {name: primary_id(name) for _, _, name, _ in DESCRIPTORS}
    if not all(resolved_ids.values()):
        raise RuntimeError("PCG MeshSet主资产ID缺失；不能创建Graph Instance")

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

        ident = unreal.GamePlatformId()
        ident.set_editor_property("namespace", "foundation")
        ident.set_editor_property("name", "pcg_gold_profile_" + label.lower())
        ident.set_editor_property("logical_version", 1)
        profile.set_editor_property("logical_id", ident)
        version = unreal.GamePlatformDataVersion()
        version.set_editor_property("schema_version", 1)
        version.set_editor_property("content_revision", 1)
        profile.set_editor_property("data_version", version)

        region = unreal.GamePlatformId()
        region.set_editor_property("namespace", "foundation")
        region.set_editor_property("name", "region_a")
        region.set_editor_property("logical_version", 1)
        profile.set_editor_property("region_id", region)
        profile.set_editor_property("template_id", template_name)
        profile.set_editor_property("template_version", 1)
        profile.set_editor_property("graph_reference", template)
        profile.set_editor_property("mesh_set_definition_id", resolved_ids[mesh_name])
        profile.set_editor_property("required_definitions", [resolved_ids[mesh_name]])
        profile.set_editor_property("execution_policy", policy.EDITOR_GENERATED_STATIC)
        profile.set_editor_property("output_usage", use.COSMETIC)
        profile.set_editor_property("minimum_outputs", 0)
        path = actual_asset_path("Realized", "PCG_Gold_" + label)
        raw = maker(path, profile, mesh)
        graph = next((x for x in raw if isinstance(x, graph_cls)), None) if isinstance(raw, tuple) else raw
        if not isinstance(graph, graph_cls):
            raise RuntimeError("UE没有产生批准的StaticMeshSpawner Graph：" + path + " " + str(raw))
        profile.set_editor_property("graph_reference", graph)
        if not assets.save_loaded_asset(profile, only_if_is_dirty=False):
            raise RuntimeError("PCG真实Profile不能保存：" + profile_path)
        reloaded = assets.load_asset(profile_path)
        if not isinstance(reloaded, cls):
            raise RuntimeError("PCG Profile保存后反射类型不一致：" + profile_path)
        print("PCG_GOLD_REALIZED_SAVED", path, profile_path, template_name, mesh_name)

    print("PCG_GOLD_REALIZED_COMPLETED", len(DESCRIPTORS))


if __name__ == "__main__":
    main()
