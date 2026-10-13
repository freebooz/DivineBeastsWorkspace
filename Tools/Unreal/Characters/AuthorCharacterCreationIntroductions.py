# -*- coding: utf-8 -*-
"""由正式UE5.8 Monolith editor.run_python执行十二生肖介绍资产的增量作者操作。

英雄介绍归现有DBAGameplay定义；技能介绍归DBAUIPack_Core的独立CreationProfile，
不把/Game/Development技能配置拖入前端包，不修改默认技能集合、授权、数值或协议身份。
草案文案来自独立CharacterCreationProfiles.json，描述已实现/未实现边界；不把设计数值当作运行效果。
界面明确标记候选，前端显示不构成策划批准或生产授权。
先备份十二份英雄定义；既有创建介绍资产必须独立审核，默认拒绝覆盖。只保存本次资产。
"""
import hashlib
import json
from pathlib import Path
import shutil
import unreal

PROFILE_ROOT = "/DBAUIPack_Core/UI/Profiles/CharacterCreation"


def author():
    """读取审查用源数据并保存真实DataAsset；缺字段、缺图标、身份重复或脏包即失败。"""
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/DivineBeastsArena.uproject") or not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("仅支持锁定正式神兽联盟UE5.8工程")
    if unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() or unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
        raise RuntimeError("存在未保存包，拒绝作者操作")
    root = Path(unreal.Paths.project_dir()).resolve().parent
    heroes = json.loads((root / "Tools/Unreal/Characters/ZodiacPlaceholderCharacters.json").read_text(encoding="utf-8"))["heroes"]
    draft = json.loads((root / "Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json").read_text(encoding="utf-8"))
    profiles = json.loads((root / "Tools/Unreal/Characters/CharacterCreationProfiles.json").read_text(encoding="utf-8"))["Profiles"]
    if draft["Status"] != "DevelopmentDraftNotApproved" or draft["ShippingEligible"] is not False:
        raise RuntimeError("技能源表状态不符，禁止把候选作为正式授权")
    evidence = root / "Saved/Validation/CharacterEntryFollowUp20261012/IntroductionData"
    backup = evidence / "Before"
    if backup.exists():
        raise RuntimeError("本轮介绍备份已存在，拒绝覆盖证据或重复新建")
    plan = []
    for hero in heroes:
        suffix = hero["heroId"].split(".")[-1]
        text = hero.get("IntroductionZh", "").strip()
        if not text:
            raise RuntimeError("作者英雄介绍缺失：" + suffix)
        path = PROFILE_ROOT + "/DA_DBA_CharacterCreation_" + suffix
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError("已存在介绍资源，须先审核引用，不覆盖：" + path)
        descriptions = [p for p in profiles if p["HeroDefinitionId"] == hero["heroId"]]
        if len(descriptions) != 1 or descriptions[0]["AssetPath"] != path or descriptions[0]["bDescriptionsAreDraft"] is not True:
            raise RuntimeError("介绍作者映射不唯一或候选声明失效：" + suffix)
        entries = descriptions[0]["Entries"]
        if len(entries) != 5 or len({e["AbilityId"] for e in entries}) != 5:
            raise RuntimeError("英雄技能草案身份或槽位不完整：" + suffix)
        expected_ids = {e["DevelopmentAbilityId"] for e in draft["Entries"] if e["HeroDefinitionId"] == hero["heroId"]}
        if {e["AbilityId"] for e in entries} != expected_ids or any("开发候选，未作为正式技能批准" not in e["DescriptionZh"] for e in entries):
            raise RuntimeError("介绍身份与草案不符或正文遗漏开发限定：" + suffix)
        definition = unreal.load_asset("/DBAGameplay/Definitions/DA_Hero_Zodiac_" + suffix)
        if not definition or str(definition.get_editor_property("definition_id")) != hero["heroId"]:
            raise RuntimeError("已有英雄身份不符：" + suffix)
        icons = [unreal.load_asset(e["IconAssetPath"]) for e in entries]
        if not all(isinstance(icon, unreal.Texture2D) for icon in icons):
            raise RuntimeError("已批准图标资源缺失：" + suffix)
        plan.append((hero, suffix, text, path, entries, definition, icons))
    backup.mkdir(parents=True)
    report = {"Tool": "Monolith MCP", "DraftOnly": True, "Assets": [], "Backups": []}
    for hero, suffix, text, path, entries, definition, icons in plan:
        disk = root / ("Game/Plugins/DivineBeasts/DBAGameplay/Content/Definitions/DA_Hero_Zodiac_" + suffix + ".uasset")
        shutil.copy2(disk, backup / disk.name)
        report["Backups"].append({"Source": str(disk), "SHA256": hashlib.sha256(disk.read_bytes()).hexdigest()})
    for hero, suffix, text, path, entries, definition, icons in plan:
        definition.set_editor_property("introduction", unreal.Text(text))
        if not unreal.EditorAssetLibrary.save_loaded_asset(definition, only_if_is_dirty=False):
            raise RuntimeError("保存英雄介绍失败：" + suffix)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.DivineBeastsCharacterCreationProfile)
        profile = unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.split("/")[-1], PROFILE_ROOT, None, factory)
        if not profile:
            raise RuntimeError("创建介绍主资产失败：" + path)
        profile.set_editor_property("hero_definition_id", unreal.Name(hero["heroId"]))
        profile.set_editor_property("descriptions_are_draft", True)
        rows = []
        for entry, icon in zip(entries, icons):
            row = unreal.DivineBeastsAbilityUIEntry()
            row.set_editor_property("ability_id", unreal.Name(entry["AbilityId"]))
            row.set_editor_property("display_name", unreal.Text(entry["DisplayNameZh"]))
            row.set_editor_property("description", unreal.Text(entry["DescriptionZh"]))
            row.set_editor_property("icon", icon)
            rows.append(row)
        profile.set_editor_property("entries", rows)
        if not unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False):
            raise RuntimeError("保存技能介绍失败：" + path)
        report["Assets"].append({"Hero": hero["heroId"], "HeroDefinition": definition.get_path_name(), "Profile": profile.get_path_name(), "SkillCount": len(rows)})
        (evidence / "Authoring.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("CHARACTER_CREATION_INTRODUCTIONS_SAVED", len(report["Assets"]))


author()
