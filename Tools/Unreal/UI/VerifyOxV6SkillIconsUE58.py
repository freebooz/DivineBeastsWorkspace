# -*- coding: utf-8 -*-
"""UE5.8丑牛V6图标冷启动回读（只读），禁止将静态文件假报成真实技能栏运行验收。"""
from __future__ import annotations
import json
import os
from pathlib import Path

import unreal

ROOT=Path(__file__).resolve().parents[3]
M=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json"
manifest=json.loads(M.read_text(encoding="utf-8-sig"))
rows=manifest["items"]
assert len(rows)==8
strict=os.environ.get("SSLM_OX_V6_VERIFY_STRICT","0")=="1"
asset_manager=unreal.AssetRegistryHelpers.get_asset_registry()
asset_manager.scan_paths_synchronous(["/DBAHeroPack_Ox"],True)
library=unreal.EditorAssetLibrary
profile=library.load_asset(manifest["currentOxPlaceholderUIProfile"])
if not isinstance(profile,unreal.DivineBeastsAbilityUIProfile):
    raise RuntimeError("Ox原生唯一技能UIProfile未载入")
if str(profile.get_editor_property("hero_definition_id"))!="Hero.Zodiac.Ox":
    raise RuntimeError("Ox原生UIProfile英雄身份错误")
entries=profile.get_editor_property("entries")
placeholders={str(a.get_editor_property("ability_id")) for a in entries if str(a.get_editor_property("ability_id")).startswith("dba.ability.ox_placeholder_")}
if len(placeholders)<6:
    raise RuntimeError("原项目六项占位技能UI条目被意外删改")
count=0
for item in rows:
    tex=library.load_asset(item["targetTexturePath"]) if library.does_asset_exist(item["targetTexturePath"]) else None
    if tex is None and not strict:
        print("SSLM_OX_V6_ICON_NOT_IMPORTED",item["displayNameZh"],item["targetTexturePath"])
        continue
    if not isinstance(tex,unreal.Texture2D):
        raise RuntimeError("图标必须为真实UE Texture2D："+item["targetTexturePath"])
    checks={
        "compression_settings":unreal.TextureCompressionSettings.TC_EDITOR_ICON,
        "lod_group":unreal.TextureGroup.TEXTUREGROUP_UI,
        "mip_gen_settings":unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS,
        "srgb":True,
        "never_stream":True,
    }
    for prop,expected in checks.items():
        if tex.get_editor_property(prop)!=expected:
            raise RuntimeError("图标导入参数错误："+item["artFileName"]+" "+prop)
    count+=1
    print("SSLM_OX_V6_ICON_COLD_OK",item["displayNameZh"],item["targetTexturePath"])
if strict and count!=8:
    raise RuntimeError("严格验收要求全部八个原生Texture2D")
if manifest["manualReviewStatus"]=="APPROVED":
    ability_ids={str(e.get_editor_property("ability_id")) for e in entries}
    if any(item["abilityId"] not in ability_ids for item in rows):
        raise RuntimeError("美术已批准，但Ox唯一UIProfile没有全部八条V6映射")
else:
    print("SSLM_OX_V6_HUMAN_APPROVAL_PENDING_UNGRANTED",len(placeholders))
print("SSLM_OX_V6_COLD_REVIEW_RESULT",count,8,"strict",strict)
