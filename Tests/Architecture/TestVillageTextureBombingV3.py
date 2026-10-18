#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 新手村地形V3.0纹理轰炸静态与原生资产证据门禁。

必须核对项目唯一色板、既有55节点地形母材质/唯一正式地图身份、平台三层
单向依赖、Texture Bombing基础色/法线/ORM的统一参数协议及冷热回读状态。
离线源码合格≠引擎Shader合格/客户端GPU/人工画面；--require-native仅验原生证据。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
AUTHOR=ROOT/"Tools/Unreal/Terrain/AuthorVillageTextureBombingV3UE58.py"
REGISTRY=ROOT/"Docs/Art/SSLMColorRegistry_V3.json"
MAPPING=ROOT/"Tools/Unreal/Terrain/TerrainV3ColorMapping.json"
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset"
MAP=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap"
REPORT=ROOT/"Game/Saved/Validation/VillageTerrainBombingV3/AssetProof.json"
BACKUP=ROOT/"Saved/ArtReview/VillageTerrainBombingV3/Before/BackupManifest.json"

def checksum(p:Path)->str:
    h=hashlib.sha256()
    with p.open("rb") as file:
        for item in iter(lambda:file.read(1024*1024),b""):h.update(item)
    return h.hexdigest()

def check(require_native:bool)->None:
    palette=json.loads(REGISTRY.read_text(encoding="utf-8"))
    mapping=json.loads(MAPPING.read_text(encoding="utf-8"))
    items={c["semanticId"]:c for c in palette["items"]}
    assert items["Environment.Grass.Standard"]["referenceSRGBHex"]=="#6B9956", "失去V3.0草地参考色"
    assert items["Environment.Rock.GrayCyan"]["referenceSRGBHex"]=="#89988C", "失去灰青岩石参考色"
    assert any(l["layer"]=="Grass" and l["semanticId"]=="Environment.Grass.Standard"
               for l in mapping["layers"]), "V3扫描纹理未使用唯一全局色板"
    assert len(mapping["layers"])==10
    assert MAP.is_file() and SOURCE.is_file(), "正式Village真实地图和Landscape桥不存在"
    author=AUTHOR.read_text(encoding="utf-8")
    for value in (
        'Engine_MaterialFunctions01/Texturing/Texture_Bombing',
        'VillageGroundAlbedoTexture','VillageGroundNormalTexture','VillageGroundORMTexture',
        'VillageGroundBombEnabled','VillageGroundBombTiling','VillageGroundBombOffset',
        'MaterialExpressionTextureObjectParameter','MaterialExpressionStaticSwitchParameter',
        'MaterialExpressionQualitySwitch','MaterialExpressionFeatureLevelSwitch',
        'get_material_expression_input_names','get_inputs_for_material_expression',
        'IsNormalmap','Engine','recompile_material','save_loaded_asset',
        'coldVerifyPassed','mapSHA256','sourceSha256'):
        assert value in author, f"缺少真正原生材质合约或质量降级能力：{value}"
    assert 'delete_material_expression(' not in author
    assert 'set_editor_property("landscape_material"' not in author
    assert 'GamePlatformSurface/MaterialFunctions' not in author
    assert 'SkyAtmosphere' not in author
    if not BACKUP.is_file() or not REPORT.is_file():
        if require_native:raise AssertionError("缺少正式材质UE apply/备份的原生证据")
        print("SSLM_VILLAGE_BOMB_STATIC_PASS native_edit=pending original_map_preserved_to_be_verified")
        return
    backup=json.loads(BACKUP.read_text(encoding="utf-8"))
    current=json.loads(REPORT.read_text(encoding="utf-8"))
    mat_copy=BACKUP.parent/SOURCE.name
    map_copy=BACKUP.parent/MAP.name
    assert mat_copy.is_file() and map_copy.is_file()
    assert checksum(mat_copy)==backup["sourceSHA256"]==current["sourceSha256"]
    assert checksum(map_copy)==backup["mapSHA256"]==current["mapSha256"]
    assert checksum(MAP)==current["mapSha256"], "正式地图意外变化"
    assert checksum(SOURCE)==current["materialSha256"], "母材质原生结果被外部覆盖"
    assert len(current["roles"])==3
    assert not current["humanReviewPassed"] and not current["gpuPassed"] and not current["cookPassed"]
    if require_native:assert current["coldVerifyPassed"], "UE独立冷重载尚未通过"
    print("SSLM_VILLAGE_BOMB_NATIVE_EVIDENCE_PASS channels=3 map_unchanged=true UE_cold_verified="+str(current["coldVerifyPassed"]).lower())

if __name__=="__main__":
    ap=argparse.ArgumentParser(description="SSLM地形纹理轰炸只读门禁")
    ap.add_argument("--require-native",action="store_true",help="要求真实UE冷重载回读成功")
    args=ap.parse_args()
    check(args.require_native)
