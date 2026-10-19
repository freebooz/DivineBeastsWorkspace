# -*- coding: utf-8 -*-
"""SSLM Ox V6 唯一UIProfile与8张Texture2D独立UE5.8冷回读（只读）。

确认稳定AbilityId、中文名、8个真实48px图标、原六占位条目完整；
同时只读确认原占位AbilitySet未因美术接线被授予任何ox_eight_未审批技能。
没有服务端授予时ViewModel不应渲染V6可施放槽，不能凭此脚本声称PIE已在HUD可见。
"""
from __future__ import annotations
import json
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
MANIFEST = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json"
m = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
rows = m["items"]
if len(rows) != 8 or m["heroId"] != "Hero.Zodiac.Ox":
    raise RuntimeError("牛V6图标映射源数据错误")

unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    ["/DBAHeroPack_Ox", "/DBAGameplay"], True)
lib=unreal.EditorAssetLibrary
path="/DBAHeroPack_Ox/UI/Abilities/DA_DBA_Ox_PlaceholderUIProfile"
profile=lib.load_asset(path)
if not isinstance(profile,unreal.DivineBeastsAbilityUIProfile):
    raise RuntimeError("丑牛原生唯一UIProfile未冷启动加载成功")
if str(profile.get_editor_property("hero_definition_id")) != "Hero.Zodiac.Ox":
    raise RuntimeError("牛UIProfile唯一主资产的HeroDefinitionId错误")
entries=list(profile.get_editor_property("entries"))
ids=[str(e.get_editor_property("ability_id")) for e in entries]
if len(ids)!=len(set(ids)):
    raise RuntimeError("技能UIProfile出现重复稳定AbilityId")
placeholders=[x for x in ids if x.startswith("dba.ability.ox_placeholder_")]
if len(placeholders)!=6 or len(entries)!=14:
    raise RuntimeError(f"原六项占位或新增八项V6未完整保留 old={len(placeholders)} total={len(entries)}")

def ui_icon_path(entry):
    icon=entry.get_editor_property("icon")
    if not icon:
        return ""
    try:
        return str(icon.get_path_name()).split(".")[0]
    except Exception:
        return str(icon)

for row in rows:
    ability=row["abilityId"]
    idx=ids.index(ability) if ability in ids else -1
    if idx<0:
        raise RuntimeError("V6真实技能缺UI表现条目："+ability)
    entry=entries[idx]
    if str(entry.get_editor_property("display_name")) != row["displayNameZh"]:
        raise RuntimeError("正式中文技能名称与V6来源不一致："+ability)
    if not str(entry.get_editor_property("description")).strip():
        raise RuntimeError("V6技能说明不能为空："+ability)
    tex=lib.load_asset(row["targetTexturePath"])
    if not isinstance(tex,unreal.Texture2D):
        raise RuntimeError("V6真实二维纹理缺失："+row["targetTexturePath"])
    icon_path=ui_icon_path(entry)
    if row["targetTexturePath"] not in icon_path:
        raise RuntimeError("V6数据资产图标未正确指向纹理："+ability+" => "+icon_path)
    # UE5.8 Python反射在不同引擎版本可暴露 Blueprint_GetSizeX 或 GetSizeX；均需读真实UTexture2D。
    def image_size(axis):
        for method in ("blueprint_get_size_"+axis, "get_size_"+axis):
            func=getattr(tex,method,None)
            if callable(func):
                try:
                    return int(func())
                except Exception:
                    continue
        raise RuntimeError("引擎没有公开真实Texture2D尺寸接口："+axis)
    if image_size("x")!=48 or image_size("y")!=48:
        raise RuntimeError("UE原生Texture2D纹理未使用48x48："+ability)
    print("SSLM_OX_V6_PROFILE_ICON_COLD_OK",row["order"],ability,row["displayNameZh"],
          row["targetTexturePath"])

# 从真实既有占位AbilitySet只读取Granted AbilityId；新增8项美术映射绝不能修改这条权威链。
ability_set = lib.load_asset("/DBAGameplay/Abilities/Placeholders/DA_DBA_Ox_PlaceholderAbilitySet")
if not isinstance(ability_set,unreal.GamePlatformAbilitySetDefinition):
    raise RuntimeError("丑牛原占位AbilitySet真实资产缺失或类错误")
grants=ability_set.get_editor_property("abilities")
for grant in grants:
    name=str(grant.get_editor_property("ability_id"))
    if name in {row["abilityId"] for row in rows} or "ox_eight_" in name:
        raise RuntimeError("未审批V6技能被误授权到占位AbilitySet："+name)
print("SSLM_OX_V6_PROFILE_COLD_VERIFY_OK",len(rows),"placeholders",len(placeholders),
      "profileEntries",len(entries),"unapprovedGrants",0)
