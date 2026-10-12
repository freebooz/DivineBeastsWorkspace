#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读UE5.8多层地形PBR资产审计：真正Texture2D×49、UMaterial×1、MI×1。

这是项目资产加载/材质父类/资源所属包的基础门禁，不能代表UE高度图已导入或
Landscape LayerInfo/PCG图已产生；没有Shader错误日志也不代表GPU性能达标。
需要在正式DivineBeastsArena项目UnrealEditor-Cmd PythonScriptPlugin中运行。
不修改任何资产或共享新手村地图。任何缺失/类型/父类/非法纯客户端硬依赖拒绝。
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=Path(__file__).with_name("TerrainSourceManifest_v1.json")
PRODUCTION=Path(__file__).with_name("TerrainProductionPBRManifest_v1.json")
MASTER="/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10"
INSTANCE="/DBAWorldPack_Village/Terrain/Materials/MI_DBA_Terrain_Calibration"

def packages() -> list[str]:
    spec=json.loads(SOURCE.read_text(encoding="utf-8"))
    targets=[]
    for item in spec["files"]:
        if PRODUCTION.exists() and item["category"]=="PBR_TECHNICAL":
            continue  # V1地形校准投影清单继续保持旧SHA，真实PBR使用新独立真源。
        kind=item["category"]
        if kind=="HEIGHT_R16_CALIBRATION":
            continue
        if kind=="PBR_TECHNICAL":
            path=item["assetPath"]
        elif kind=="GAEA_COMPATIBLE_SAMPLE_MASK":
            path="/DBAWorldPack_Village/Terrain/Calibration/Masks/T_DBA_GaeaCalib_"+item["maskName"]
        elif kind=="LANDSCAPE_WEIGHT":
            path="/DBAWorldPack_Village/Terrain/Calibration/Weights/T_DBA_LayerWeight_"+item["layer"]
        else:
            raise ValueError("未知Terrain项目SourceArt内容身份："+kind)
        if not path.startswith("/DBAWorldPack_Village/Terrain/"):
            raise ValueError("平台目录不得硬持有具体神兽联盟贴图："+path)
        targets.append(path)
    if PRODUCTION.exists():
        scanned=json.loads(PRODUCTION.read_text(encoding="utf-8"))
        if scanned.get("sourceKind")!="official_polyhaven_CC0_matched_scan_maps":
            raise RuntimeError("PBR真实来源类型非法")
        targets.extend(i["assetPath"] for i in scanned["files"])
    expected=59 if PRODUCTION.exists() else 49
    if len(targets)!=expected or len(set(targets))!=expected:
        raise RuntimeError("真实Texture2D目录项数不符或重复："+str(len(targets)))
    return targets

def run()->None:
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("必须由真实UE Editor加载.uasset，不可按磁盘名判断") from exc
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不是正式项目")
    lib=unreal.EditorAssetLibrary
    for asset in packages():
        if not lib.does_asset_exist(asset) or not isinstance(lib.load_asset(asset),unreal.Texture2D):
            raise RuntimeError("UE Texture2D真实反射资源缺失："+asset)
    mat=lib.load_asset(MASTER)
    inst=lib.load_asset(INSTANCE)
    if not isinstance(mat,unreal.Material) or not isinstance(inst,unreal.MaterialInstanceConstant):
        raise TypeError("10层PBR主材质与实例的UE真实类型不正确")
    if inst.get_editor_property("parent")!=mat:
        raise RuntimeError("项目实例没有继承对应Material母材质")
    count=unreal.MaterialEditingLibrary.get_num_material_expressions(mat)
    if count<140:
        raise RuntimeError("十层BaseColor/Normal/ORM及天气、Fallback节点不完整")
    if not lib.does_asset_exist("/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"):
        raise RuntimeError("已有正式Village地形桥不能因新增多层材质消失")
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    opts=unreal.AssetRegistryDependencyOptions()
    opts.set_editor_property("include_hard_package_references",True)
    opts.set_editor_property("include_soft_package_references",False)
    opts.set_editor_property("include_searchable_names",False)
    opts.set_editor_property("include_hard_management_references",False)
    opts.set_editor_property("include_soft_management_references",False)
    deps=[str(x) for x in registry.get_dependencies(MASTER,opts)]
    forbidden=[x for x in deps if x.startswith(("/GamePlatformSurface/","/GamePlatformVFX/","/DBAPresentationPack_Core/"))]
    if forbidden:
        raise RuntimeError("项目共享母材质非法硬引用纯客户端平台资源："+str(forbidden))
    print("TERRAIN_UE_ASSET_INVENTORY_PASS textures="+str(len(packages()))+" material=1 material_instance=1",
          "expression_count",count)
    print("TERRAIN_UE_ASSET_SERVER_SAFE_MATERIAL_DEPENDENCIES_PASS",
          "dependencies",len(deps),"forbidden_client_only",len(forbidden))
    print("TERRAIN_UE_PENDING LayerInfo=10 LandscapeImport=1009x1009 PCGGraph_Generation=unverified")

if __name__=="__main__":
    run()
