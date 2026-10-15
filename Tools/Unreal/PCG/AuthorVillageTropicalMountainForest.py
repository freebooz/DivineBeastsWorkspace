#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 新手村外围热带森林原生PCG候选点图：由UE5.8编辑器创建。
归属项目层 DBAWorldPack_Village，复用平台 GamePlatformPCG。
输入为有界Landscape及LS_ForestSoil地貌权重；只输出点，不生成未获资源的树木。
不修改正式Village地图，不覆盖用户资产；失败则阻断。
"""
import argparse
import json
from pathlib import Path

CONFIG = "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/PCG/TropicalMountainForest_v1.json"
OUTPUT = "/DBAWorldPack_Village/PCG/Graphs"
CLASSES = ("PCGGetLandscapeSettings", "PCGSurfaceSamplerSettings",
           "PCGAttributeFilteringSettings", "PCGNormalToDensitySettings",
           "PCGDensityFilterSettings")


def read_contract(root):
    """读取三层生成参数；值为编辑器开发初值，不能当作生产效果验收。"""
    doc = json.loads((root / CONFIG).read_text(encoding="utf-8"))
    if doc["projectCode"] != "SSLM" or doc["status"]["releaseReady"]:
        raise RuntimeError("项目身份或交付状态不符")
    if doc["contentOwner"] != "DBAWorldPack_Village":
        raise RuntimeError("PCG森林项目资源归属不符")
    if not doc["worldScope"]["boundedVolumeRequired"]:
        raise RuntimeError("外围森林禁止无界生成")
    if doc["terrain"]["layerName"] != "LS_ForestSoil":
        raise RuntimeError("森林土壤层必须复用原有Landscape图层")
    for layer in doc["layers"]:
        if not (0 < layer["samplingDensityPerSquareMeter"] <= .005):
            raise RuntimeError("PCG候选点密度不合规")
    return doc


def check_graph(ue, graph, layer):
    """反射回读真实PCG节点/阈值，拒绝旧资源静默覆盖。"""
    if not isinstance(graph, ue.PCGGraph):
        raise TypeError("资产必须为UE原生PCGGraph")
    nodes = graph.get_editor_property("nodes")
    actual = tuple(n.get_settings().get_class().get_name() for n in nodes)
    if actual != CLASSES or len(graph.get_all_edges()) != 7:
        raise RuntimeError("PCG节点与七条边不符：" + str(actual))
    if not graph.get_editor_property("landscape_uses_metadata"):
        raise RuntimeError("未开启地表图层元数据")
    land, sample, soil, normal, slope = [n.get_settings() for n in nodes]
    if land.get_editor_property("unbounded"):
        raise RuntimeError("森林图不能无界采样")
    if not land.get_editor_property("sampling_properties").get_editor_property("get_layer_weights"):
        raise RuntimeError("未开启Landscape LayerWeights")
    if abs(sample.get_editor_property("points_per_squared_meter") -
           layer["samplingDensityPerSquareMeter"]) > .000001:
        raise RuntimeError("采样密度不符")
    if str(soil.get_editor_property("target_attribute").get_attribute_name()) != "LS_ForestSoil":
        raise RuntimeError("森林属性非LS_ForestSoil")
    if soil.get_editor_property("operator") != ue.PCGAttributeFilterOperator.GREATER_OR_EQUAL:
        raise RuntimeError("森林权重过滤运算符有误")
    if abs(soil.get_editor_property("attribute_types").get_editor_property("float_value") -
           layer["minForestSoilWeight"]) > .00001:
        raise RuntimeError("森林权重阈值有误")
    if abs(normal.get_editor_property("normal").z - 1.) > .00001:
        raise RuntimeError("法线的世界Z轴有误")
    if abs(slope.get_editor_property("lower_bound") -
           layer["minNormalZ"]) > .00001:
        raise RuntimeError("山峰坡面筛选阈值有误")


def author_graph(ue, layer):
    """UE资产工具创建项目层图，只有在全部引脚核实后保存。"""
    name = layer["pcgAsset"]
    target = OUTPUT + "/" + name
    lib = ue.EditorAssetLibrary
    if lib.does_asset_exist(target):
        check_graph(ue, lib.load_asset(target), layer)
        print("VILLAGE_FOREST_GRAPH_EXISTS_VERIFIED", target)
        return
    if not lib.does_directory_exist(OUTPUT):
        lib.make_directory(OUTPUT)
    g = ue.AssetToolsHelpers.get_asset_tools().create_asset(
        name, OUTPUT, ue.PCGGraph, ue.PCGGraphFactory())
    if not isinstance(g, ue.PCGGraph):
        raise RuntimeError("UE创建原生PCG图失败")
    g.set_editor_property("landscape_uses_metadata", True)
    g.set_editor_property("is_template", False)
    land, l = g.add_node_of_type(ue.PCGGetLandscapeSettings)
    sampler, sample = g.add_node_of_type(ue.PCGSurfaceSamplerSettings)
    soil, f = g.add_node_of_type(ue.PCGAttributeFilteringSettings)
    normal, n = g.add_node_of_type(ue.PCGNormalToDensitySettings)
    slope, d = g.add_node_of_type(ue.PCGDensityFilterSettings)
    l.set_editor_property("unbounded", False)
    if not l.get_editor_property("sampling_properties").get_editor_property("get_layer_weights"):
        raise RuntimeError("UE Landscape不能读取Weight层")
    sample.set_editor_property("points_per_squared_meter",
                               float(layer["samplingDensityPerSquareMeter"]))
    sample.set_editor_property("apply_density_to_points", True)
    # UE5.8 的选择器ScriptMethod会返回bool却未修改输入副本。
    # 通过PCG临时点元数据的Last属性解析，取得真正的InputSelector并回写。
    point_data = ue.new_object(ue.PCGPointData)
    point_data.mutable_metadata().create_float_attribute(ue.Name("LS_ForestSoil"), 0., True)
    selector = ue.PCGAttributePropertyInputSelector().copy_and_fix_last(point_data)
    if str(selector.get_attribute_name()) != "LS_ForestSoil":
        raise RuntimeError("PCG候选点森林土壤选择器解析失败")
    f.set_editor_property("target_attribute", selector)
    f.set_editor_property("operator", ue.PCGAttributeFilterOperator.GREATER_OR_EQUAL)
    f.set_editor_property("use_constant_threshold", True)
    values = f.get_editor_property("attribute_types")
    values.set_editor_property("type", ue.PCGMetadataTypes.FLOAT)
    values.set_editor_property("float_value", float(layer["minForestSoilWeight"]))
    f.set_editor_property("attribute_types", values)
    n.set_editor_property("normal", ue.Vector(0, 0, 1))
    n.set_editor_property("strength", 1.)
    n.set_editor_property("offset", 0.)
    d.set_editor_property("lower_bound", float(layer["minNormalZ"]))
    d.set_editor_property("upper_bound", 1.)
    edges = [(g.get_input_node(), "In", land, "BoundingShape"),
             (land, "Out", sampler, "Surface"),
             (g.get_input_node(), "In", sampler, "Bounding Shape"),
             (sampler, "Out", soil, "In"),
             (soil, "InsideFilter", normal, "In"),
             (normal, "Out", slope, "In"),
             (slope, "Out", g.get_output_node(), "Out")]
    for start, out_pin, end, in_pin in edges:
        if g.add_edge(start, ue.Name(out_pin), end, ue.Name(in_pin)) != end:
            raise RuntimeError("UE PCG节点引脚连接失败：" + out_pin)
    check_graph(ue, g, layer)
    if not lib.save_loaded_asset(g, only_if_is_dirty=False):
        raise RuntimeError("UE未保存PCG图")
    check_graph(ue, lib.load_asset(target), layer)
    print("VILLAGE_FOREST_NATIVE_PCG_SOURCE_SAVED", target)


def main(mode):
    if mode == "inspect":
        contract = read_contract(Path(__file__).resolve().parents[3])
        for x in contract["layers"]:
            print("VILLAGE_FOREST_SOURCE_PLAN", OUTPUT + "/" + x["pcgAsset"])
        return
    import unreal as ue  # type: ignore
    project = Path(ue.Paths.get_project_file_path().replace("\\", "/"))
    if not str(project).replace("\\", "/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE Editor并非SSLM主工程")
    doc = read_contract(project.parent.parent)
    if mode == "verify":
        for layer in doc["layers"]:
            path = OUTPUT + "/" + layer["pcgAsset"]
            check_graph(ue, ue.EditorAssetLibrary.load_asset(path), layer)
            print("VILLAGE_FOREST_GRAPH_RELOADED", path)
        return
    for layer in doc["layers"]:
        author_graph(ue, layer)
    print("ELECTRIC_DREAMS_NOT_IMPORTED_FOREST_NOT_BAKED")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"), default="inspect")
    main(parser.parse_args().mode)
