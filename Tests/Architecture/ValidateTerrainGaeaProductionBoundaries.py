#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟 Gaea×Landscape×PCG三层逻辑/素材资产/UE制作入口静态质量门禁。

仅只读运行，针对当前UE5.8工作区已实施资源：
- GamePlatformSurfaceEditor 与 GamePlatformPCGEditor实现统一作者工具，不新建ProjectPCG插件；
- 50张技术源图、49张UE包文件、1母材质+1实例、4个PCG Blueprint、1采样Graph；
- 16位高程与十层权重的R16/R8内容哈希和原生UE作者工具对应版本；
- 真正Gaea生产文件不自动捏造，尚未完成地图和LayerInfo时如实标记待验收。
此门禁不替代UE真实UClass反射、P4过滤Graph执行、GPU与双Cook。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VILLAGE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village"
TERRAIN = VILLAGE / "Content/Terrain"
RAW = VILLAGE / "SourceArt/Terrain/CalibrationDerived/Raw"
SRC = ROOT / "Tools/Unreal/Terrain"
SURFACE_EDITOR = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceEditor"
PCG_EDITOR = ROOT / "Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor"
LAYERS = ("Grass","ForestSoil","Dirt","Rock","Scree","Sand","Gravel","RoadDirt","FarmSoil","ShoreMud")

def require(value: bool, meaning: str) -> None:
    if not value:
        raise RuntimeError("Terrain架构门禁失败："+meaning)

def check_source(filename: str, *terms: str) -> None:
    path=ROOT/filename
    require(path.is_file(),"不存在已批准源码："+filename)
    source=path.read_text(encoding="utf-8-sig")
    for term in terms:
        require(term in source, filename+"缺少合同："+term)

def raw_integrity() -> tuple[int, int]:
    doc=json.loads((RAW/"TerrainUEImportRaw_v1.json").read_text(encoding="utf-8"))
    require(doc.get("sourceKind")=="calibration_only_not_gaea","不可将校准高程伪称Gaea")
    require(doc.get("schemaVersion")==1 and doc.get("gridSamplesX")==1009 and
            doc.get("gridSamplesY")==1009 and len(doc.get("layers",[]))==10,
            "地形网格与LayerInfo属性合同非法")
    require(doc["mapPath"]=="/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview",
            "独立技术地图路径更改未审批")
    files=doc.get("files",[])
    require(len(files)==11,"需要一张原生R16和10张R8")
    total=0
    for entry in files:
        path=(ROOT/entry["relativePath"]).resolve()
        require(path.is_relative_to(ROOT) and path.is_file(),"R16/R8源文件缺失或越界："+str(path))
        raw=path.read_bytes()
        require(hashlib.sha256(raw).hexdigest()==entry["sha256"],"R16/R8原始哈希失败："+str(path))
        require(len(raw)==entry["bytes"],"原生地形源文件长度错误："+str(path))
        total+=len(raw)
    return len(files),total

def main() -> None:
    require((ROOT/"AGENTS.md").is_file(),"仓库工程规范缺失")
    require((ROOT/"Docs/Architecture/TerrainMaskDataContract_V1_20261011.md").is_file(),"地形数据合同缺失")
    require((ROOT/"Docs/Implementation/TerrainGaeaMultiLayerPCGExecutionPlan_20261011.md").is_file(),"执行计划未入库")
    require(not any(TERRAIN.rglob("*.uplugin")),"禁止在世界内容包新增平行ProjectPCG插件")
    check_source("Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceEditor/GamePlatformSurfaceEditor.Build.cs",
                 '"Landscape"', '"UnrealEd"')
    check_source("Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceEditor/Private/Commands/GamePlatformSurfaceTerrainLayersCommandlet.cpp",
                 "ULandscapeLayerInfoObject","SetLayerName","FinalWeightBlending","SavePackage","ValidateOnly")
    check_source("Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source/GamePlatformSurfaceEditor/Private/Commands/GamePlatformSurfaceTerrainReviewMapCommandlet.cpp",
                 "ALandscape","Import(","ExpectedLayerCount","ELandscapeImportAlphamapType::Additive",
                 "UEditorLoadingAndSavingUtils::SaveMap","/Game/Development/Terrain/Maps/")
    check_source("Game/Plugins/GamePlatform/World/GamePlatformPCG/Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGTerrainBiomeGraphsCommandlet.cpp",
                 "UPCGGetLandscapeSettings","UPCGSurfaceSamplerSettings","UPCGAttributeFilteringSettings",
                 "bGetLayerWeights=true","TargetAttribute.SetAttributeName","AttributeTypes.FloatValue")
    check_source("Tools/Unreal/Terrain/RunTerrainGaeaFullReview.ps1",
                 "UnrealEditor.exe","UnrealBuildTool","Run-UeCommandlet","-ValidateOnly")
    source_manifest=json.loads((SRC/"TerrainSourceManifest_v1.json").read_text(encoding="utf-8"))
    require(len(source_manifest.get("files",[]))==50 and
            source_manifest.get("sourceKind")=="procedural_calibration_not_gaea",
            "50张技术源图和来源声明不一致")
    for spec in source_manifest["files"]:
        file=(ROOT/spec["sourcePath"]).resolve()
        require(file.is_relative_to(ROOT) and file.is_file(),"SourceArt不存在："+str(file))
        require(hashlib.sha256(file.read_bytes()).hexdigest()==spec["sha256"],
                "SourceArt已变化，拒绝继续："+str(file))
    tex=tuple(TERRAIN.rglob("*.uasset"))
    textures=[f for f in tex if "Textures" in f.parts or "Calibration" in f.parts]
    require(len(textures)>=49,"UE Texture2D实际文件少于49")
    require((TERRAIN/"Materials/M_DBA_Terrain_MultiLayer10.uasset").stat().st_size>30000,
            "真实多层UMaterial未生成")
    require((TERRAIN/"Materials/MI_DBA_Terrain_Calibration.uasset").stat().st_size>1000,
            "真实项目MI尚未保存")
    bps=tuple((TERRAIN/"PCG/Blueprints").glob("BP_DBA_Terrain_*Biome.uasset"))
    require(len(bps)==4,"项目PCG地貌放置器蓝图应有4份")
    graph=TERRAIN/"PCG/Graphs/PCG_DBA_Terrain_LandscapeSemanticSampling.uasset"
    require(graph.is_file() and graph.stat().st_size>10000,"原生PCG地形元数据输入图未保存")
    nr,bytes_count=raw_integrity()
    # 输出绝不将源码已写/技术数据验证成功，误描述成正式Landscape和Biome过滤图已完成。
    layers=tuple((TERRAIN/"LayerInfos").glob("LI_DBA_Terrain_*.uasset"))
    review_map=ROOT/"Game/Content/Development/Terrain/Maps/L_DBA_TerrainMaskReview.umap"
    filters=tuple((TERRAIN/"PCG/Graphs").glob("PCG_DBA_Terrain_*WeightFiltered.uasset"))
    print("TERRAIN_GAEA_THREE_TIER_STATIC_PASS source_png=50 ue_source_packages=",
          len(textures),"raw_count",nr,"raw_bytes",bytes_count,
          "pcg_blueprints",len(bps),"pcg_input_graph=1")
    print("TERRAIN_GAEA_EDITOR_GATES pending_layer_infos",10-len(layers),
          "qa_map_exists",int(review_map.is_file()),
          "biome_filter_graphs",len(filters),"expected_biome_filter_graphs=4")
    print("TERRAIN_GAEA_PROVENANCE_STATUS calibration_only_no_verified_gaea_2_3_export")

if __name__ == "__main__":
    main()
