# -*- coding: utf-8 -*-
"""SSLM未羊V6：仅在Monolith MCP连接正式UE5.8时写入技能栏只读预览顺序。

职责：对已经导入的8张真实Texture2D和唯一AbilityUIProfile逐项核对，
验证旧条目不变，安全备份后只设置ArtPreviewAbilityIds（八个稳定技能编号）。
不创建新的UIProfile、不改变现有V5名称、不发放GAS、不修改其他生肖。
调用前：StageGoatV6IconsFromArchive.ps1的stage、真实Texture2D导入、
CompleteImportedHeroIconProfilesMonolith的apply/verify已完成。
"""
from pathlib import Path
import json
import os
import shutil
import unreal

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat"
OUT = ROOT / "Saved/Validation/GoatV6IconImport20261020"
PROFILE_PATH = "/DBAHeroPack_Goat/UI/Abilities/DA_DBA_Goat_PlaceholderUIProfile"
PROFILE_FILE = PACK / "Content/UI/Abilities/DA_DBA_Goat_PlaceholderUIProfile.uasset"
MODE = str(globals().get("GOAT_PREVIEW_MODE", "inspect")).lower()

def ensure(condition, message):
    """阻止加载错误资产、越权改名及不确定的重复保存。"""
    if not condition:
        raise RuntimeError("GoatV6Preview: " + message)

def entries_snapshot(profile):
    """保存原始UI条目所需的完整展示身份，方便人工恢复和对比。"""
    rows = []
    for entry in list(profile.get_editor_property("entries")):
        icon = entry.get_editor_property("icon")
        icon_id = icon.get_path_name().split(".")[0] if icon else ""
        rows.append({"abilityId": str(entry.get_editor_property("ability_id")),
                     "name": str(entry.get_editor_property("display_name")),
                     "description": str(entry.get_editor_property("description")),
                     "icon": icon_id})
    return rows

def main():
    """只读检查、写预览槽、独立核验分别显式执行。"""
    ensure(MODE in ("inspect", "apply", "verify"), "不支持的执行模式")
    ensure(Path(unreal.Paths.get_project_file_path()).resolve() ==
           (ROOT / "Game/DivineBeastsArena.uproject").resolve(), "工程不匹配")
    ensure(unreal.SystemLibrary.get_engine_version().startswith("5.8."), "UE版本不匹配")
    specs = json.loads((PACK/"Docs/GoatV6_ArtFreeze_20261020.json").read_text(encoding="utf-8"))
    plan = json.loads((OUT/"IntegrationPlan.json").read_text(encoding="utf-8"))
    ensure(specs["heroId"] == "Hero.Zodiac.Goat" and len(specs["skills"]) == 8,
           "必须使用已冻结的八技能")
    hero = plan["heroes"][0]
    ensure(len(plan["heroes"]) == 1 and hero["heroId"] == "Hero.Zodiac.Goat" and
           hero["profilePath"] == PROFILE_PATH and len(hero["entries"]) == 8,
           "唯一英雄UIProfile计划不正确")
    lib = unreal.EditorAssetLibrary
    profile = lib.load_asset(PROFILE_PATH)
    ensure(isinstance(profile, unreal.DivineBeastsAbilityUIProfile), "原生Profile缺失")
    ensure(str(profile.get_editor_property("hero_definition_id")) == "Hero.Zodiac.Goat",
           "现有Profile绑定了其他英雄")
    old_entries = entries_snapshot(profile)
    by_id = {x["abilityId"]: x for x in old_entries}
    ensure(len(by_id) == len(old_entries), "原有Profile存在重复技能ID")
    requested = []
    for skill, target in zip(specs["skills"], hero["entries"]):
        ability = skill["abilityId"]
        ensure(ability == target["abilityId"] and
               skill["productionV5NameZh"] == target["displayNameZh"] and
               skill["texturePath"] == target["texturePath"],
               "正式V5名称或V6身份发生变化")
        src = by_id.get(ability)
        ensure(src is not None and src["name"] == target["displayNameZh"] and
               src["icon"] == target["texturePath"] and bool(src["description"].strip()),
               "必须先经通用UIProfile工具写入真实映射：" + ability)
        ensure(isinstance(lib.load_asset(skill["texturePath"]), unreal.Texture2D),
               "真实48像素Texture2D缺失：" + skill["texturePath"])
        requested.append(ability)
    ensure(len(set(requested)) == 8, "预览技能ID重复")
    current = [str(x) for x in profile.get_editor_property("art_preview_ability_ids")]
    ensure(not current or current == requested,
           "原Profile已有不相同的预览技能集，需要人工审查")
    changed = False
    if MODE == "apply" and current != requested:
        ensure(PROFILE_FILE.is_file(), "无法备份原生Profile文件")
        backup = OUT/"Backups/Goat/GoatBeforeV6PreviewOrder.uasset"
        snapshot = OUT/"Backups/Goat/GoatBeforeV6PreviewOrder.json"
        ensure(not backup.exists() and not snapshot.exists(), "已有备份，不能覆盖恢复点")
        backup.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PROFILE_FILE, backup)
        snapshot.write_text(json.dumps(old_entries, ensure_ascii=False, indent=2),
                            encoding="utf-8")
        profile.set_editor_property("art_preview_ability_ids",
                                    [unreal.Name(x) for x in requested])
        ensure(lib.save_loaded_asset(profile, only_if_is_dirty=False),
               "Profile保存失败")
        ensure(all(x in entries_snapshot(profile) for x in old_entries),
               "写入预览顺序意外改变了原有条目")
        changed = True
    if MODE == "verify":
        ensure(current == requested, "八个只读预览槽位尚未绑定")
    OUT.mkdir(parents=True,exist_ok=True)
    result = {"mode":MODE, "heroId":"Hero.Zodiac.Goat", "entryCount":len(old_entries),
              "previewCount":len(requested), "changed":changed,
              "currentlyBound":current == requested or changed,
              "hasGrantedV6Gameplay":False, "onlineHUDVerified":False}
    (OUT/("GoatV6PreviewOrder_"+MODE+".json")).write_text(
        json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps(result,ensure_ascii=False))

if __name__ == "__main__":
    main()
