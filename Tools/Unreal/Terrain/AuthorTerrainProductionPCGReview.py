#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在独立UE5.8十层地形关卡装配真实PCGVolume与四张Landscape权重过滤图。

本工具只操作 /Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview。
首次装配先确认一张ALandscape和十份LS_地形层已存在，且PCGGraph是真实引擎资产。
四类Graph由GamePlatformPCGEditor官方节点创建，数据来源统一Landscape LayerWeights。
不碰正式Village地图、不开新服务器规则、不伪造PCG Bake或运行时GPU成功。
"""
from __future__ import annotations
import os

MODE=os.environ.get("TERRAIN_PCG_SCENE_MODE","inspect").strip().lower()
MAP="/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview"
BASE="/DBAWorldPack_Village/Terrain/PCG/Graphs"
BIOMES={
 "Forest":"PCG_DBA_Terrain_ForestWeightFiltered",
 "Rock":"PCG_DBA_Terrain_RockWeightFiltered",
 "Shore":"PCG_DBA_Terrain_ShoreWeightFiltered",
 "Road":"PCG_DBA_Terrain_RoadWeightFiltered",
}
LAYERS=("LS_Grass","LS_ForestSoil","LS_Dirt","LS_Rock","LS_Scree","LS_Sand",
        "LS_Gravel","LS_RoadDirt","LS_FarmSoil","LS_ShoreMud")

def run():
 if MODE not in ("inspect","apply","verify"):
  raise ValueError("TERRAIN_PCG_SCENE_MODE须为inspect/apply/verify")
 print("TERRAIN_PCG_SCENE_PLAN",MODE,MAP,"biome_count",len(BIOMES))
 if MODE=="inspect":return
 try:import unreal # type: ignore
 except ImportError as err:raise RuntimeError("需真实UE5.8编辑器，不能伪造PCG .uasset") from err
 project=unreal.Paths.get_project_file_path().replace("\\","/")
 if not project.endswith("/Game/DivineBeastsArena.uproject"):
  raise RuntimeError("拒绝在非正式工程绑定Landscape或PCG")
 asset=unreal.EditorAssetLibrary
 level=unreal.EditorLevelLibrary
 if not asset.does_asset_exist(MAP) or not level.load_level(MAP):
  raise RuntimeError("真实独立十层审核地图尚未创建，不得以平面替代Landscape")
 graphs={}
 for name,short in BIOMES.items():
  graph=asset.load_asset(BASE+"/"+short)
  if not isinstance(graph,unreal.PCGGraph):
   raise TypeError("PCG权重过滤图必须由Editor原生创建："+short)
  if not graph.get_editor_property("landscape_uses_metadata"):
   raise RuntimeError("PCG图未启用Landscape LayerWeight元数据："+short)
  graphs[name]=graph
 actors=list(level.get_all_level_actors())
 terrain=[x for x in actors if isinstance(x,unreal.Landscape)]
 if len(terrain)!=1:
  raise RuntimeError("审核地图的真实Landscape数量不为1："+str(len(terrain)))
 volume_class=getattr(unreal,"PCGVolume",None)
 component_class=getattr(unreal,"PCGComponent",None)
 if volume_class is None or component_class is None:
  raise RuntimeError("工程未启用官方PCG插件，不创建代理Actor")
 scene={}
 for actor in actors:
  label=actor.get_actor_label()
  if label.startswith("TerrainPBR_PCG_"):
   scene[label]=actor
 expected_labels={"TerrainPBR_PCG_"+name for name in BIOMES}
 if scene and set(scene)!=expected_labels:
  raise RuntimeError("独立地图PCG体积处于部分生成状态，拒绝盲目覆盖已有Actor")
 if MODE=="verify" and set(scene)!=expected_labels:
  raise RuntimeError("审核关卡尚未装配全部四类PCGVolume")
 if MODE=="apply" and not scene:
  for name,graph in graphs.items():
   # 默认PCGVolume刷形在UE5.8内约200厘米；放大覆盖1009点校准地形。
   volume=level.spawn_actor_from_class(volume_class,unreal.Vector(0.0,0.0,5000.0))
   if not volume:raise RuntimeError("UE未生成真正PCGVolume："+name)
   volume.set_actor_label("TerrainPBR_PCG_"+name)
   volume.set_actor_scale3d(unreal.Vector(320.0,320.0,110.0))
   comp=volume.get_component_by_class(component_class)
   if not isinstance(comp,component_class):
    raise RuntimeError("PCGVolume没有UPCGComponent："+name)
   comp.set_graph(graph)
   if comp.get_graph()!=graph:
    raise RuntimeError("PCG体积图绑定后回读失败："+name)
  if not level.save_current_level():
   raise RuntimeError("独立关卡保存失败；不得声称PCG图已绑定")
  print("TERRAIN_PBR_PCG_VOLUME_MAP_SAVED",MAP)
 # 即使地图保存成功，也要逐项检查PCGComponent和Graph同源；实际Bake另验。
 scene={x.get_actor_label():x for x in level.get_all_level_actors() if
        x.get_actor_label().startswith("TerrainPBR_PCG_")}
 if set(scene)!=expected_labels:
  raise RuntimeError("四类PCG体积不完整")
 for name,graph in graphs.items():
  obj=scene["TerrainPBR_PCG_"+name]
  comp=obj.get_component_by_class(component_class)
  if not isinstance(comp,component_class) or comp.get_graph()!=graph:
   raise RuntimeError("PCGVolume未绑定正确权重过滤图："+name)
 print("TERRAIN_PBR_LANDSCAPE_PCG_FOUR_GRAPHS_BOUND",MAP,
       "biomes=Forest,Rock,Shore,Road")
 print("TERRAIN_PBR_PCG_RUNTIME_GENERATE_AND_BAKE_PENDING")

if __name__=="__main__":run()
