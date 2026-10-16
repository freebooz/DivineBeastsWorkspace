#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""十类正式扫描PBR三层/许可/资源隔离静态门禁，给出独立UE验收的实际缺口。

此脚本只读取项目文件；SHA完整并不能证明UE已导入、Shader编译通过、
PCG完成Bake、雪苔在GPU上可见或服务器Cook实际剥离纯客户端材质。
资产内容归第三层WorldPack，正式项目母材质归纯表现包，平台机制不反向依赖项目。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[2]
WORLD=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
PRESENT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
PLATFORM=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface"
TERRAIN=WORLD/"Content/Terrain"
BUILD=ROOT/"Tools/Unreal/Terrain"
MANIFEST=BUILD/"TerrainProductionPBRManifest_v1.json"
HISTORY=BUILD/"TerrainSourceManifest_v1.json"
MAP=ROOT/"Game/Content/Development/Terrain/Maps/L_DBA_TerrainMaskReview.umap"
LIVE=WORLD/"Content/Maps/L_Village_Start.umap"
REQUIRED={"Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud"}
ROLES={"BaseColor","Normal","ORM","Height"}

def ensure(ok,message):
 if not ok:raise RuntimeError("正式地形PBR三层合同违规："+message)

def hash_file(path):
 h=hashlib.sha256()
 with path.open("rb") as stream:
  for part in iter(lambda:stream.read(1<<20),b""):h.update(part)
 return h.hexdigest()

def run():
 p=json.loads(MANIFEST.read_text(encoding="utf-8"))
 old=json.loads(HISTORY.read_text(encoding="utf-8"))
 ensure(p.get("sourceKind")=="official_polyhaven_CC0_matched_scan_maps","正式来源不是Poly Haven CC0扫描")
 ensure(old.get("sourceKind")=="procedural_calibration_not_gaea","不能改写历史Gaea校准来源")
 ensure(len(p["files"])==40,"正式十种地表必须各有4个物理通道")
 names={}
 for spec in p["files"]:
  layer,role=spec["layer"],spec["role"]
  ensure(layer in REQUIRED and role in ROLES,"出现未知层或通道")
  names.setdefault(layer,set()).add(role)
  source=(ROOT/spec["sourcePath"]).resolve()
  ensure(source.is_relative_to(ROOT) and source.is_file(),"PBR源文件缺失或越界")
  ensure(hash_file(source)==spec["sha256"],"PBR源SHA256损坏："+str(source))
  ensure(spec["originUrl"]=="https://polyhaven.com/a/"+spec["slug"],"第三方扫描来源身份不符")
 ensure(set(names)==REQUIRED and all(roles==ROLES for roles in names.values()),"PBR物理图不齐")
 tech=[x for x in old["files"] if x["category"]=="PBR_TECHNICAL"]
 geo=[x for x in old["files"] if x["category"]!="PBR_TECHNICAL"]
 ensure(len(tech)==30 and len(geo)==20,"历史地貌清单记录数量异常")
 for spec in geo:
  f=ROOT/spec["sourcePath"]
  ensure(f.is_file() and hash_file(f)==spec["sha256"],"地貌校准高程/掩码/权重已发生漂移")
 backup=(ROOT/p["rollbackArchive"]).resolve()
 ensure(backup.is_file(),"旧技术纹理和UE资产的回退包缺失")
 with zipfile.ZipFile(backup) as z:
  ensure(z.testzip() is None and len(z.namelist())==61,"回退ZIP存在校验失败")
  rel=HISTORY.relative_to(ROOT).as_posix()
  ensure(hashlib.sha256(z.read(rel)).hexdigest()==hash_file(HISTORY),"回退中的V1历史校准清单不一致")
 ensure(LIVE.is_file(),"正式Village地图丢失")
 data=LIVE.read_bytes()
 ensure(b"M_DBA_Village_LandscapeBridge" in data,"正式Village尚未保留原有中性天气材质路径")
 ensure(b"M_DBA_Terrain_Production10" not in data,"正式Village不应提前硬引用高精客户端材质")
 surf=json.loads((PLATFORM/"GamePlatformSurface.uplugin").read_text(encoding="utf-8-sig"))
 ensure(all(m["Type"] in ("ClientOnly","Editor") for m in surf["Modules"]),
        "平台表面表现模块不能进入专服目标")
 script=(BUILD/"AuthorTerrainProductionMaterial.py").read_text(encoding="utf8")
 # 从平台C++唯一真源读取MPC真实名称，禁止遗漏GP_Surface_前缀。
 native=(PLATFORM/"Source/GamePlatformSurfaceClient/Private/Types/GamePlatformSurfaceParameterNames.cpp").read_text(encoding="utf-8")
 for term in ("GP_Surface_GlobalSnowAmount","GP_Surface_GlobalWetness","GP_Surface_GlobalMossInfluence"):
  ensure(term in native and ('global_scalar("'+term+'"') in script,
         "项目PBR未引用真实MPC参数："+term)
 for term in ("MPC_GP_SurfaceGlobal","TerrainEnableHeightBlend",
              "MP_AMBIENT_OCCLUSION","SSM_WRAP_WORLD_GROUP_SETTINGS"):
  ensure(term in script,"地形PBR缺少平台参数或共用采样器合同："+term)
 workflow=(BUILD/"RunTerrainGaeaFullReview.ps1").read_text(encoding="utf-8-sig")
 for term in ("ProductionPBR","AuthorTerrainProductionPCGReview.py",
              "GamePlatformPCGTerrainBiomeGraphs","GamePlatformSurfaceTerrainReviewMap"):
  ensure(term in workflow,"缺少原生UE/PCG集成步骤："+term)
 ensure("-Module=GamePlatformSurfaceEditor+GamePlatformPCGEditor" in workflow and
        "'-NoUBA'" in workflow,
        "地形任务构建必须限Surface/PCG编辑器模块，避免再次重链整个UE引擎")
 graph_cmd=(ROOT/"Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGTerrainBiomeGraphsCommandlet.cpp").read_text(encoding="utf-8")
 ensure("/*bShouldStopOnSeparator=*/false" in graph_cmd and "BiomeMap=" in graph_cmd,
        "PCG四类Biome逗号串可能被FParse截断，禁止只生成第一张Graph")
 ensure("Forest:LS_ForestSoil,Rock:LS_Rock,Shore:LS_ShoreMud,Road:LS_RoadDirt" in workflow,
        "项目四类PCG Graph到地形层的映射不完整")
 # 正式扫描独立审核使用V2地形：道路与通用排除不能借用历史V1的同义图层。
 ensure("CalibrationDerivedV2\\Raw" in workflow and "projectionVersion -ne 2" in workflow,
        "高精十层审查场景未启用显式V2原始权重")
 v2raw=WORLD/"SourceArt/Terrain/CalibrationDerivedV2/Raw"
 v2doc=json.loads((v2raw/"TerrainUEImportRaw_v1.json").read_text(encoding="utf-8"))
 ensure(v2doc.get("projectionVersion")==2 and
        v2doc.get("sourceKind")=="calibration_only_not_gaea" and
        len(v2doc.get("files",[]))==11,
        "V2原生权重导入合同缺失或混入未批准Gaea")
 for entry in v2doc["files"]:
  f=(ROOT/entry["relativePath"]).resolve()
  ensure(f.is_relative_to(ROOT) and f.is_file() and f.stat().st_size==entry["bytes"]
         and hash_file(f)==entry["sha256"], "V2原始文件损坏："+entry["relativePath"])
 road=v2raw/"Weights/LS_RoadDirt.r8"
 ensure(not any(road.read_bytes()), "V2未提供道路叠加源，RoadDirt必须全零")

 landscape_cmd=(ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceEditor/Private/Commands/GamePlatformSurfaceTerrainReviewMapCommandlet.cpp").read_text(encoding="utf-8")
 ensure('TEXT("LayerIds="),LayerIds,' in landscape_cmd and
        "/*bShouldStopOnSeparator=*/false" in landscape_cmd,
        "Landscape十层CSV参数不能被默认分隔符截断")
 scene=(BUILD/"AuthorTerrainProductionPCGReview.py").read_text(encoding="utf-8")
 for term in ("GamePlatformPCGVolumeActor","generation_bounds","set_box_extent",
              "pcg_component","20261012+index"):
  ensure(term in scene,"PCG独立审核关卡未复用平台放置器/真实生成边界："+term)
 for term in ("SAMPLERTYPE_LINEAR_GRAYSCALE","SAMPLERTYPE_NORMAL","SAMPLERTYPE_MASKS"):
  ensure(term in script,"UE5.8 PBR Shader采样器物理类型不完整："+term)
 print("TERRAIN_PRODUCTION_PBR_PROVENANCE_AND_3TIER_STATIC_PASS","scan_layers=10","maps=40",
       "calibration_geo=20","rollback_members=61","client_mpc_contract=3")
 actual=[x for x in p["files"] if
         (WORLD/"Content/Terrain/Textures"/(x["layer"]+"/T_DBA_Terrain_"+x["layer"]+"_"+x["role"]+".uasset")).is_file()]
 print("TERRAIN_PRODUCTION_UE_TEXTURE_PACKAGES_ON_DISK",len(actual),"required=40",
       "type_and_dimensions_need_editor_reflection")
 filters=list((TERRAIN/"PCG/Graphs").glob("PCG_DBA_Terrain_*WeightFiltered.uasset"))
 print("TERRAIN_PRODUCTION_PCG_FILTER_GRAPH_PACKAGES",len(filters),"expected=4",
       "actual_bake_need_editor")
 print("TERRAIN_PRODUCTION_LANDSCAPE_QA_MAP",int(MAP.is_file()),
       "real_world_reopen_and_visual_manual_review_pending")
 print("TERRAIN_PRODUCTION_VILLAGE_SHARED_MAP_PROTECTED yes")
 print("TERRAIN_PRODUCTION_STATUS source_complete UE_shader_scene_cook_manual_pending")

if __name__=="__main__":run()
