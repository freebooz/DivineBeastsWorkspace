#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8原生Texture2D批量导入与只读复查：十层地表PBR+地貌/权重掩码。

只有源Manifest是已签SHA的可复现Calibration技术素材才导入新资产；
不允许覆盖世界包已有Texture2D、不改正式Village材质或地图。
应从真正Unreal Editor PythonScriptPlugin执行（或经Editor Monolith调用），
默认TERRAIN_TEXTURE_IMPORT_MODE=inspect；设为apply才真正导入；
=verify只加载并验证全部49张UE Texture2D资产。
TerrainHeight16仍作为外部16位真实Landscape高度源，不虚假当作Texture2D高度已导入。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = Path(__file__).with_name("TerrainSourceManifest_v1.json")
PRODUCTION = Path(__file__).with_name("TerrainProductionPBRManifest_v1.json")
MODE = os.getenv("TERRAIN_TEXTURE_IMPORT_MODE","inspect").strip().lower()
PREFIX = "/DBAWorldPack_Village/Terrain"


def read_targets() -> list[dict]:
    doc=json.loads(MANIFEST.read_text(encoding="utf-8"))
    if doc.get("schemaVersion")!=1 or doc.get("sourceKind")!="procedural_calibration_not_gaea":
        raise ValueError("地形源资源身份/版本不正确")
    targets=[]
    names=set()
    production=PRODUCTION.exists()  # 正式扫描PBR只能由独立ReimportTerrainProductionPBR导入。
    for spec in doc["files"]:
        if production and spec["category"]=="PBR_TECHNICAL":
            continue  # 保留历史V1来源指纹，不尝试从已替换的PNG按旧签名导入。
        src=(ROOT / spec["sourcePath"]).resolve()
        if not src.is_relative_to(ROOT) or not src.is_file():
            raise FileNotFoundError("项目外或不存在的地形原图："+str(src))
        if hashlib.sha256(src.read_bytes()).hexdigest()!=spec["sha256"]:
            raise ValueError("地貌源图签名不匹配："+str(src))
        kind=spec["category"]
        if kind=="HEIGHT_R16_CALIBRATION":
            print("TERRAIN_HEIGHT_REQUIRES_LANDSCAPE_IMPORT_NOT_TEXTURE2D",src)
            continue
        if kind=="PBR_TECHNICAL":
            package=spec["assetPath"]
            compression=spec["compression"]
            srgb=spec["sRGB"]
        elif kind=="GAEA_COMPATIBLE_SAMPLE_MASK":
            package=PREFIX+"/Calibration/Masks/T_DBA_GaeaCalib_"+spec["maskName"]
            compression,srgb="Masks",False
        elif kind=="LANDSCAPE_WEIGHT":
            package=PREFIX+"/Calibration/Weights/T_DBA_LayerWeight_"+spec["layer"]
            compression,srgb="Masks",False
        else:
            raise ValueError("未知Terrain资产类型："+kind)
        if not package.startswith(PREFIX+"/") or package in names:
            raise ValueError("不允许越界/重复的UE纹理资源路径："+package)
        names.add(package)
        targets.append({"source":src,"target":package,"srgb":srgb,"compression":compression,
                        "kind":kind})
    expected=19 if production else 49
    if len(targets)!=expected:
        raise RuntimeError("地形校准Texture2D清单数量错误："+str(len(targets))+" expected="+str(expected))
    return targets


def run() -> None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("TERRAIN_TEXTURE_IMPORT_MODE仅支持inspect/apply/verify")
    targets=read_targets()
    if MODE=="inspect":
        print("TERRAIN_TEXTURE_PLAN_COUNT",len(targets))
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("不能在普通Python中伪造UE Texture2D") from exc
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE未加载正式神兽联盟工程："+project)
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    factory={
        "Default":unreal.TextureCompressionSettings.TC_DEFAULT,
        "Normalmap":unreal.TextureCompressionSettings.TC_NORMALMAP,
        "Masks":unreal.TextureCompressionSettings.TC_MASKS,
    }
    pending=[]
    for spec in targets:
        target=spec["target"]
        if assets.does_asset_exist(target):
            obj=assets.load_asset(target)
            if not isinstance(obj,unreal.Texture2D):
                raise TypeError("已有同名资源不是Texture2D："+target)
            print("TERRAIN_TEXTURE_ALREADY_EXISTS_SKIPPING",target)
            continue
        if MODE=="verify":
            raise FileNotFoundError("真实UE Texture2D尚未生成："+target)
        folder,name=target.rsplit("/",1)
        if not assets.does_directory_exist(folder):
            assets.make_directory(folder)
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(spec["source"]))
        task.set_editor_property("destination_path",folder)
        task.set_editor_property("destination_name",name)
        task.set_editor_property("automated",True)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("save",False)
        pending.append((spec,task))
    if pending:
        # UE在此阶段创建真Texture2D对象，但尚未保存；后续逐个检查类别及材质压缩。
        tools.import_asset_tasks([task for _,task in pending])
    for spec,task in pending:
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError("UE导入地表Texture2D没有实际创建："+spec["target"])
        obj=assets.load_asset(spec["target"])
        if not isinstance(obj,unreal.Texture2D):
            raise TypeError("UE导入的资源不是Texture2D："+spec["target"])
        obj.set_editor_property("srgb",bool(spec["srgb"]))
        obj.set_editor_property("compression_settings",factory[spec["compression"]])
        if not assets.save_loaded_asset(obj,only_if_is_dirty=False):
            raise RuntimeError("地形Texture2D的原生SavePackage失败："+spec["target"])
        print("TERRAIN_TEXTURE_UE_SAVED",spec["target"])
    for spec in targets:
        obj=assets.load_asset(spec["target"])
        if not isinstance(obj,unreal.Texture2D):
            raise TypeError("UE纹理回读失败："+spec["target"])
    print("TERRAIN_TEXTURE_UE_VERIFY_PASS",len(targets),
          "newlyCreated",len(pending),"mode",MODE)
if __name__=="__main__":
    run()
