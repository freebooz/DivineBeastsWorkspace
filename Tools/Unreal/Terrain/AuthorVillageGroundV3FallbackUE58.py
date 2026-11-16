#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM新手村V3草地基础色低容量默认资产（UE5.8原生）制作与绑定。

仅创建一张256x256的WorldPack共享sRGB Texture2D（纹理资产），将已有
VillageGroundAlbedoTexture（地表基础色）同名两节点默认纹理同时改为此资产。
客户端运行时仍通过GamePlatformData软租约覆盖为完整2K V3来源，保障游戏质量；
专用服务器Shared Landscape没有项目表现插件的硬依赖。
保留已保存的81个Texture_Bombing/原PBR/雪湿润母材质节点与地图。
执行前独占检查、指纹备份；apply编辑后必须独立UE进程verify，不能仅看退出码0。
"""
from __future__ import annotations
import hashlib
import json
import os
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MODE=os.getenv("SSLM_VILLAGE_V3_FALLBACK_MODE","inspect").strip().lower()
MASTER="/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
FALLBACK="/DBAWorldPack_Village/Materials/Landscape/Defaults/T_DBA_Village_GroundAlbedoV3Default"
MASTER_FILE=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
                  "Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset")
MAP_FILE=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
               "Content/Maps/L_Village_Start.umap")
SRC_MANIFEST=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
                   "SourceArt/Terrain/BombingV3/VillageBombFallbackManifest.json")
ASSET_PROOF=ROOT/"Game/Saved/Validation/VillageTerrainBombingV3/AssetProof.json"
BACKUP=ROOT/"Saved/ArtReview/VillageTerrainBombingV3/BeforeFallback"


def sha(path:Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):h.update(chunk)
    return h.hexdigest()


def inspect_source():
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("模式仅支持inspect/apply/verify")
    if not SRC_MANIFEST.is_file() or not ASSET_PROOF.is_file():
        raise RuntimeError("尚无V3共享草地降采样源或真实Texture Bombing原生报告")
    src=json.loads(SRC_MANIFEST.read_text(encoding="utf-8-sig"))
    proof=json.loads(ASSET_PROOF.read_text(encoding="utf-8"))
    source=ROOT/src["outputSourcePath"]
    if src["schemaVersion"]!=1 or src["width"]!=256 or src["height"]!=256 or not source.is_file():
        raise RuntimeError("256低成本共享草地源不完整")
    if sha(source)!=src["outputSha256"]:
        raise RuntimeError("共享草地源PNG的SHA不可信")
    if sha(MAP_FILE)!=proof["mapSha256"]:
        raise RuntimeError("本任务不得保存或覆盖L_Village_Start共享地图")
    if MODE!="verify" and (not proof["coldVerifyPassed"] or proof["expressionCount"]<80):
        raise RuntimeError("母材质还未通过真实UE的Texture Bombing独立冷重载")
    if MODE=="apply" and sha(MASTER_FILE)!=proof["materialSha256"]:
        raise RuntimeError("共享Landscape材质已被其他任务修改，拒绝覆盖")
    return source,proof


def unreal_context():
    import unreal  # type: ignore
    if not str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE工程上下文错误")
    return unreal


def make_asset(unreal,source:Path, allow_import:bool):
    lib=unreal.EditorAssetLibrary
    if not lib.does_asset_exist(FALLBACK):
        if not allow_import:raise RuntimeError("UE共享低分辨率Grass默认纹理尚未创建")
        dest,name=FALLBACK.rsplit("/",1)
        if not lib.does_directory_exist(dest):lib.make_directory(dest)
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(source))
        task.set_editor_property("destination_path",dest)
        task.set_editor_property("destination_name",name)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("automated",True)
        task.set_editor_property("save",False)
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        if not task.get_editor_property("imported_object_paths"):
            raise RuntimeError("256低分辨率纹理没有UE ImportAssetTask实际对象")
        obj=lib.load_asset(FALLBACK)
        if not isinstance(obj,unreal.Texture2D):
            raise RuntimeError("UE导入结果不是Texture2D")
        obj.set_editor_property("srgb",True)
        obj.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_DEFAULT)
        if not lib.save_loaded_asset(obj,only_if_is_dirty=False):
            raise RuntimeError("UE共享低分辨率纹理保存失败")
    tex=lib.load_asset(FALLBACK)
    if not isinstance(tex,unreal.Texture2D) or not bool(tex.get_editor_property("srgb")):
        raise RuntimeError("新Village默认Grass纹理无sRGB/Texture2D证明")
    if tex.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT:
        raise RuntimeError("新Village默认Grass纹理压缩设置不正确")
    data=lib.find_asset_data(FALLBACK)
    if not data.is_valid() or str(data.get_tag_value("Dimensions") or "")!="256x256":
        raise RuntimeError("新共享草地纹理的UE原生尺寸不是256x256")
    provenance=tex.get_editor_property("asset_import_data")
    imported=str(provenance.get_first_filename()).replace("\\","/").casefold() if provenance else ""
    if imported!=str(source).replace("\\","/").casefold():
        raise RuntimeError("低分辨率共享Texture2D没有保留正确来源PNG")
    return tex


def nodes(unreal,mat):
    return [x for x in unreal.MaterialEditingLibrary.get_material_expressions(mat)
            if isinstance(x,(unreal.MaterialExpressionTextureSampleParameter2D,
                             unreal.MaterialExpressionTextureObjectParameter))
            and str(x.get_editor_property("parameter_name"))=="VillageGroundAlbedoTexture"]


def apply(unreal,source:Path,proof:dict):
    if BACKUP.exists():
        raise RuntimeError("已有本轮更新前备份，请先核查，不允许重复覆盖")
    target_sha=sha(MASTER_FILE)
    BACKUP.mkdir(parents=True,exist_ok=False)
    for current,copy in ((MASTER_FILE,BACKUP/MASTER_FILE.name),(MAP_FILE,BACKUP/MAP_FILE.name)):
        shutil.copy2(current,copy)
        if sha(current)!=sha(copy):raise RuntimeError("共享原生资源备份SHA不一致")
    (BACKUP/"BackupManifest.json").write_text(json.dumps({
        "date":"2026-10-18","materialBeforeFallbackSha256":target_sha,
        "mapSha256":sha(MAP_FILE),"sourceZh":"原Texture Bombing母材质二次默认纹理升级前备份"
    },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    tex=make_asset(unreal,source,allow_import=True)
    lib=unreal.EditorAssetLibrary
    mat=lib.load_asset(MASTER)
    if not isinstance(mat,unreal.Material):
        raise RuntimeError("正式地形母材质不能由UE加载")
    if unreal.MaterialEditingLibrary.get_num_material_expressions(mat)!=proof["expressionCount"]:
        raise RuntimeError("此前纹理轰炸真实材质节点被其他任务改变")
    found=nodes(unreal,mat)
    if len(found)!=2:
        raise RuntimeError("地表基础色必须由单采样和Bombing纹理对象两个同名参数共同控制")
    for node in found:node.set_editor_property("texture",tex)
    messages=[str(x) for x in unreal.MaterialEditingLibrary.recompile_material(mat) or []]
    if any("error" in x.lower() for x in messages):
        raise RuntimeError("V3共享默认颜色接入导致Shader报错，拒绝保存："+str(messages[:6]))
    if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
        raise RuntimeError("UE未成功保存带新V3默认颜色的共享地形母材质")
    proof["materialBeforeFallbackSha256"]=target_sha
    proof["materialSha256"]=sha(MASTER_FILE)
    proof["sharedV3FallbackTexture"]=FALLBACK
    proof["sharedV3FallbackSourceSha256"]=sha(source)
    proof["coldVerifyPassed"]=False
    ASSET_PROOF.write_text(json.dumps(proof,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_V3_BOMB_GROUND_FALLBACK_APPLY_SAVED",
          "new_shared_tex=256x256","material_nodes="+str(len(found)),
          "map_unchanged="+str(sha(MAP_FILE)==proof["mapSha256"]).lower(),flush=True)


def verify(unreal,source:Path,proof:dict):
    if proof.get("sharedV3FallbackTexture")!=FALLBACK or sha(MASTER_FILE)!=proof["materialSha256"]:
        raise RuntimeError("真实V3默认草地未应用到正式母材质")
    tex=make_asset(unreal,source,allow_import=False)
    mat=unreal.EditorAssetLibrary.load_asset(MASTER)
    found=nodes(unreal,mat)
    if len(found)!=2 or any(x.get_editor_property("texture")!=tex for x in found):
        raise RuntimeError("单采样和Bombing的默认源未同步")
    if unreal.MaterialEditingLibrary.get_num_material_expressions(mat)!=proof["expressionCount"]:
        raise RuntimeError("默认V3颜色编辑意外改变纹理轰炸PBR图")
    if sha(MAP_FILE)!=proof["mapSha256"]:
        raise RuntimeError("地图已经被其他进程修改")
    proof["coldVerifyPassed"]=True
    ASSET_PROOF.write_text(json.dumps(proof,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_V3_BOMB_GROUND_FALLBACK_COLD_VERIFY_PASS",
          "default_color_shared_256=true","village_map_unchanged=true",flush=True)


def main():
    source,proof=inspect_source()
    if MODE=="inspect":
        print("SSLM_V3_BOMB_GROUND_FALLBACK_PLAN source=256x256 target="+FALLBACK+
              " editable_source_only_in_WorldPack=true",flush=True)
        return
    unreal=unreal_context()
    if MODE=="apply":apply(unreal,source,proof)
    else:verify(unreal,source,proof)


if __name__=="__main__":main()
