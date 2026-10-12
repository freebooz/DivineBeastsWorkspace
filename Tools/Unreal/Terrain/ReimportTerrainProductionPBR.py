#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在正式UE5.8工程中将十层同名技术Texture2D替换为同源扫描PBR。

操作只对已批准的40张地表纹理生效；19张校准Mask/Weight、正式Village地图
以及项目之外资产不触碰。默认inspect；apply必须存在完整回退包与官方SHA签名。
正常导入与二次进程verify分开，任何失败不得上报为成功。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MANIFEST=ROOT/"Tools/Unreal/Terrain/TerrainProductionPBRManifest_v1.json"
MODE=os.environ.get("TERRAIN_PBR_UE_MODE","inspect").lower().strip()
ROLES={"BaseColor":"Default","Normal":"Normalmap","ORM":"Masks","Height":"Grayscale"}
LAYERS={"Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud"}
ASSET_ROOT="/DBAWorldPack_Village/Terrain/Textures/"

def check_source():
 if not MANIFEST.is_file():
  raise RuntimeError("正式PBR来源清单不存在：已暂存但尚未授权覆盖/完成源替换")
 p=json.loads(MANIFEST.read_text(encoding="utf-8"))
 if p.get("sourceKind")!="official_polyhaven_CC0_matched_scan_maps" or len(p.get("files",[]))!=40:
  raise RuntimeError("扫描素材元数据必须为40张官方CC0同源图")
 backup=ROOT/p["rollbackArchive"]
 if not backup.is_file() or backup.stat().st_size<200000:
  raise RuntimeError("旧引擎纹理备份不存在，不允许重导入")
 by={}
 for spec in p["files"]:
  layer,role=spec["layer"],spec["role"]
  if layer not in LAYERS or role not in ROLES or (
      spec["assetPath"]!=ASSET_ROOT+layer+"/T_DBA_Terrain_"+layer+"_"+role):
   raise ValueError("已签PBR资产包/层/角色越界")
  path=(ROOT/spec["sourcePath"]).resolve()
  if not path.is_relative_to(ROOT) or not path.is_file():
   raise RuntimeError("源PNG缺失或越界："+str(path))
  digest=hashlib.sha256(path.read_bytes()).hexdigest()
  if digest!=spec["sha256"]:
   raise ValueError("源PNG已被替换，拒绝继续："+str(path))
  by.setdefault(layer,set()).add(role)
 if len(by)!=10 or any(set(ROLES)!=roles for roles in by.values()):
  raise ValueError("至少一个物理通道缺失")
 return p["files"]

def run():
 if MODE not in ("inspect","apply","verify"):
  raise ValueError("TERRAIN_PBR_UE_MODE仅支持inspect/apply/verify")
 specs=check_source()
 if MODE=="inspect":
  print("TERRAIN_PBR_UE_PLAN scanned_maps=40 existing_overwrite=30 new_height=10")
  return
 try:import unreal # type: ignore
 except ImportError as exc:raise RuntimeError("仅可由真实UE5.8 Editor导入材质资产") from exc
 project=unreal.Paths.get_project_file_path().replace("\\","/")
 if not project.endswith("/Game/DivineBeastsArena.uproject"):
  raise RuntimeError("当前编辑器项目不是神兽联盟正式工程："+project)
 assets=unreal.EditorAssetLibrary
 tool=unreal.AssetToolsHelpers.get_asset_tools()
 factory={
  "Default":unreal.TextureCompressionSettings.TC_DEFAULT,
  "Normalmap":unreal.TextureCompressionSettings.TC_NORMALMAP,
  "Masks":unreal.TextureCompressionSettings.TC_MASKS,
  "Grayscale":unreal.TextureCompressionSettings.TC_GRAYSCALE,
 }
 to_import=[]
 if MODE=="apply":
  for spec in specs:
   asset=spec["assetPath"]
   if assets.does_asset_exist(asset):
    existing=assets.load_asset(asset)
    if not isinstance(existing,unreal.Texture2D):
     raise TypeError("同名资产不是Texture2D，拒绝覆盖："+asset)
   directory,name=asset.rsplit("/",1)
   if not assets.does_directory_exist(directory):assets.make_directory(directory)
   task=unreal.AssetImportTask()
   task.set_editor_property("filename",str(ROOT/spec["sourcePath"]))
   task.set_editor_property("destination_path",directory)
   task.set_editor_property("destination_name",name)
   task.set_editor_property("automated",True)
   task.set_editor_property("replace_existing",True)
   task.set_editor_property("save",False)
   to_import.append((spec,task))
  tool.import_asset_tasks([task for _,task in to_import])
  for spec,task in to_import:
   if not task.get_editor_property("imported_object_paths"):
    raise RuntimeError("Unreal资产替换失败："+spec["assetPath"])
   obj=assets.load_asset(spec["assetPath"])
   if not isinstance(obj,unreal.Texture2D):
    raise TypeError("Unreal实际Texture2D丢失："+spec["assetPath"])
   obj.set_editor_property("srgb",bool(spec["sRGB"]))
   obj.set_editor_property("compression_settings",factory[spec["compression"]])
   if not assets.save_loaded_asset(obj,only_if_is_dirty=False):
    raise RuntimeError("Unreal SavePackage保存失败："+spec["assetPath"])
   print("TERRAIN_PBR_UE_ASSET_SAVED",spec["layer"],spec["role"],flush=True)
 # verify在新的Editor进程中独立执行，防止误报编辑器缓存中的对象。
 for spec in specs:
  obj=assets.load_asset(spec["assetPath"])
  if not isinstance(obj,unreal.Texture2D):
   raise TypeError("Unreal资产不存在/类型不符："+spec["assetPath"])
  if obj.get_editor_property("compression_settings")!=factory[spec["compression"]]:
   raise RuntimeError("Texture Compression设置错误："+spec["assetPath"])
  if bool(obj.get_editor_property("srgb"))!=bool(spec["sRGB"]):
   raise RuntimeError("sRGB设置错误："+spec["assetPath"])
  get_width=getattr(obj,"blueprint_get_size_x",None)
  get_height=getattr(obj,"blueprint_get_size_y",None)
  if callable(get_width) and callable(get_height):
   if (get_width(),get_height())!=(2048,2048):
    raise RuntimeError("UE纹理像素尺寸并非2048："+spec["assetPath"])
 print("TERRAIN_PBR_UE_40_TEXTURES_VERIFIED mode="+MODE+" count=40",flush=True)

if __name__=="__main__":run()
