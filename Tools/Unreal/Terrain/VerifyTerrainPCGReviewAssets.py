#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8真实Landscape-PCG原生图和神兽联盟四种QA蓝图的加载/节点/参数门禁。

审核的不是单纯.uasset存在：检查PCGGraph由官方GetLandscapeData→SurfaceSampler→Output
真实4边图组成，Landscape Layer Weight Metadata明确开启，采样密度受限；验证
4份项目放置器Blueprint生成类确实派生平台基类，DomainId/WorldStage/Schema/Seed完整。
范围严格只是PCG输入Graph和Blueprint：还没有经过Gaea导入的真实Landscape/LayerInfo，
也没有MeshSet/Spawner的执行结果，禁止声称已经森林/道路/G01–G16完成。
在正式UE5.8 Editor中执行且不修改任何资产。
"""
from __future__ import annotations

ROOT="/DBAWorldPack_Village/Terrain"
GRAPH=ROOT+"/PCG/Graphs/PCG_DBA_Terrain_LandscapeSemanticSampling"
BLUEPRINTS=(
    ("BP_DBA_Terrain_ForestBiome","Forest.Canopy","SCATTER",20261011),
    ("BP_DBA_Terrain_RockBiome","Rock.Scatter","SCATTER",20261013),
    ("BP_DBA_Terrain_ShoreBiome","Water.Bank","INTERFACE_BANDS",20261017),
    ("BP_DBA_Terrain_RoadBiome","Road.Surface","NETWORKS",20261019),
)

def main() -> None:
    try:import unreal # type: ignore
    except ImportError as e:raise RuntimeError("必须由UE编辑器反射已保存Graph和Blueprint") from e
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("错误UE工程")
    lib=unreal.EditorAssetLibrary
    graph=lib.load_asset(GRAPH)
    if not isinstance(graph,unreal.PCGGraph):
        raise TypeError("项目Terrain PCGGraph不存在")
    if not graph.get_editor_property("landscape_uses_metadata"):
        raise RuntimeError("PCGGraph未启用Landscape地表权重元数据")
    nodes=graph.get_editor_property("nodes")
    if len(nodes)!=2 or len(graph.get_all_edges())!=4:
        raise RuntimeError("PCGGraph不是已批准的两节点、四条连接拓扑")
    settings=[node.get_settings() for node in nodes]
    land=next((s for s in settings if isinstance(s,unreal.PCGGetLandscapeSettings)),None)
    surf=next((s for s in settings if isinstance(s,unreal.PCGSurfaceSamplerSettings)),None)
    if not land or not surf:
        raise TypeError("真实UE Graph没有GetLandscape和SurfaceSampler两个官节点")
    if (land.get_editor_property("unbounded") or
            not land.get_editor_property("sampling_properties").get_editor_property("get_layer_weights")):
        raise RuntimeError("不能让PCG在无限世界范围采样，也必须读取LandscapeLayerWeight")
    density=surf.get_editor_property("points_per_squared_meter")
    if density <= 0 or density > .005:
        raise RuntimeError("Terrain PCG采样密度超出M1开发范围："+str(density))
    if not lib.does_asset_exist("/Game/Development/Foundation/PCG/Realized/PCG_Gold_Canopy"):
        raise RuntimeError("平台原有金标准Graph被意外移动或丢失")
    parent=unreal.GamePlatformPCGActorBase
    stage=getattr(unreal,"GamePlatformPCGWorldStage",None)
    for name,domain,stage_name,seed in BLUEPRINTS:
        package=ROOT+"/PCG/Blueprints/"+name
        cls=lib.load_blueprint_class(package)
        if not isinstance(lib.load_asset(package),unreal.Blueprint) or cls is None:
            raise TypeError("真实UE ChildBlueprint不存在："+package)
        if not unreal.MathLibrary.class_is_child_of(cls,parent):
            raise RuntimeError("项目Blueprint没有继承平台PCG放置器："+package)
        default=unreal.get_default_object(cls)
        if (str(default.get_editor_property("domain_id"))!=domain or
                int(default.get_editor_property("seed"))!=seed or
                int(default.get_editor_property("required_schema_major"))!=1 or
                default.get_editor_property("world_stage")!=getattr(stage,stage_name)):
            raise RuntimeError("项目Biome Blueprint领域/Schema/种子/阶段不匹配："+package)
        print("TERRAIN_PCG_BLUEPRINT_RELOAD_VERIFIED",name,domain,stage_name)
    print("TERRAIN_PCG_LANDSCAPE_GRAPH_RELOAD_PASS",
          "graph",GRAPH,"nodes",len(nodes),"edges",len(graph.get_all_edges()),
          "landscape_layer_weights=on","sample_points_per_sq_m",density)
    print("TERRAIN_PCG_ACTUAL_SPAWNER_BAKE_UNVERIFIED")

if __name__=="__main__":
    main()
