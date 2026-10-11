#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通过UE5.8官方PCG节点创建可读取Landscape权重的真实PCG Graph（仅Editor校准）。

所属神兽联盟第三层Village世界包，不新增ProjectPCG插件，复用GamePlatformPCG体系。
官方节点：Graph Input(In) → Get Landscape Data(BoundingShape),
Get Landscape Data(Out) → SurfaceSampler(Surface),
Graph Input(In) → SurfaceSampler(Bounding Shape),
SurfaceSampler(Out) → Graph Output(Out)。
GetLandscape.SamplingProperties.bGetLayerWeights必须为True，Graph.bLandscapeUsesMetadata=True，
SurfaceSampler限密度.004点/平方米，避免一公里景观样本爆炸。
这是能在PCG中产生点与Landscape层权重元数据的**输入图**；未绑定项目MeshSet/SpawnPolicy、
尚无真正LandscapeLayerInfo和正式Gaea高度，故不得把它称为森林生成图已完成。
默认inspect只读，apply在原生UE Editor创建、保存、回读；verify仅验证现有原生Graph。
"""

from __future__ import annotations

import os

MODE=os.getenv("TERRAIN_PCG_GRAPH_MODE","inspect").strip().lower()
GRAPH="/DBAWorldPack_Village/Terrain/PCG/Graphs/PCG_DBA_Terrain_LandscapeSemanticSampling"
MIN_NODES=2
EXPECTED_EDGES=4

def run()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("TERRAIN_PCG_GRAPH_MODE仅允许inspect/apply/verify")
    print("TERRAIN_PCG_GRAPH_PLAN",MODE,GRAPH,"landscape_weights=True","density=.004/m2")
    if MODE=="inspect":return
    try: import unreal  # type: ignore
    except ImportError as e:raise RuntimeError("PCGGraph必须由真实UE Editor创建，不可伪造") from e

    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE编辑器未加载神兽联盟唯一工程")
    lib=unreal.EditorAssetLibrary
    if lib.does_asset_exist(GRAPH):
        g=lib.load_asset(GRAPH)
        if not isinstance(g,unreal.PCGGraph):raise TypeError("已有同名非PCGGraph资源，拒绝覆盖")
        if len(g.get_editor_property("nodes"))<MIN_NODES or len(g.get_all_edges())!=EXPECTED_EDGES:
            raise RuntimeError("已有同名Graph缺少真实PCG节点及引脚边，不允许覆盖")
        if g.get_editor_property("landscape_uses_metadata") is not True:
            raise RuntimeError("Graph未启用LandscapeLayer权重Metadata")
        print("TERRAIN_PCG_GRAPH_ALREADY_VERIFIED",GRAPH,
              "nodes",len(g.get_editor_property("nodes")),
              "edges",len(g.get_all_edges()))
        return
    if MODE=="verify":
        raise FileNotFoundError("PCG项目层真实地貌输入Graph尚未生成："+GRAPH)

    folder,name=GRAPH.rsplit("/",1)
    if not lib.does_directory_exist(folder):
        lib.make_directory(folder)
    factory=unreal.PCGGraphFactory()
    graph=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,folder,unreal.PCGGraph,factory)
    if not isinstance(graph,unreal.PCGGraph):
        raise RuntimeError("UE资产工具未创建真正UPCGGraph")
    graph.set_editor_property("landscape_uses_metadata",True)
    graph.set_editor_property("is_template",False)
    graph.set_editor_property("expose_to_library",False)
    from_landscape,settings=graph.add_node_of_type(unreal.PCGGetLandscapeSettings)
    sampler,sampling=graph.add_node_of_type(unreal.PCGSurfaceSamplerSettings)
    settings.set_editor_property("unbounded",False)
    props=settings.get_editor_property("sampling_properties")
    if props.get_editor_property("get_layer_weights") is not True:
        raise RuntimeError("UE5.8 GetLandscape Data未能获取Landscape Material Layer Weights")
    sampling.set_editor_property("points_per_squared_meter",.004)
    sampling.set_editor_property("apply_density_to_points",True)
    links=[
        (graph.get_input_node(),"In",from_landscape,"BoundingShape"),
        (from_landscape,"Out",sampler,"Surface"),
        (graph.get_input_node(),"In",sampler,"Bounding Shape"),
        (sampler,"Out",graph.get_output_node(),"Out")
    ]
    for left,out_pin,right,in_pin in links:
        try:
            linked=graph.add_edge(left,unreal.Name(out_pin),right,unreal.Name(in_pin))
        except Exception as e:
            raise RuntimeError(f"官方PCGGraph引脚连接失败 {out_pin}→{in_pin}") from e
        if linked != right:
            raise RuntimeError("PCG引擎没有接纳该Graph边："+out_pin+"->"+in_pin)
    graph.set_editor_property("landscape_uses_metadata",True)
    if len(graph.get_all_edges())!=EXPECTED_EDGES:
        raise RuntimeError("UE PCGGraph边连接数不符，不可保存")
    if not lib.save_loaded_asset(graph,only_if_is_dirty=False):
        raise RuntimeError("UE Editor真实保存UPCGGraph失败")
    reloaded=lib.load_asset(GRAPH)
    if not isinstance(reloaded,unreal.PCGGraph) or len(reloaded.get_all_edges())!=EXPECTED_EDGES:
        raise RuntimeError("保存回读后PCGGraph类型或边连接不符")
    print("TERRAIN_PCG_UE_LANDSCAPE_GRAPH_SAVED",GRAPH,
          "nodes",len(reloaded.get_editor_property("nodes")),
          "edges",len(reloaded.get_all_edges()),
          "read_layer_weights=True sample_density=.004")

if __name__=="__main__":
    run()
