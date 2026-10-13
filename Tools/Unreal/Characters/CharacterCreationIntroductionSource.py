#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""创建页纯介绍源的只读依赖门禁，属于项目作者工具，不是运行时资产管理器。

输入：纯介绍JSON、原型英雄JSON、客户端INI、既有开发草案身份；输出：成功返回或中文ValueError。
严格字段白名单隔离数值/授权草案，检查12份软映射和60项英雄归属。不会导入UE或改文件，
也不证明真实DataAsset、INI反射解析、Cook或客户端已经通过。main可直接运行本文件。
"""
import json
import re
from pathlib import Path

HERO_SUFFIXES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
SLOTS = ("BasicAttack", "Passive", "Active01", "Active02", "Ultimate")
DRAFT_NOTICE = "开发候选，未作为正式技能批准"
PROFILE_ROOT = "/DBAUIPack_Core/UI/Profiles/CharacterCreation/"
SETTINGS_SECTION = "/Script/DivineBeastsUIClient.DivineBeastsUISettings"
DRAFT_SOURCE = "Docs/Implementation/ZodiacAbilityDevelopmentDraft_20261009.json"


def validate_hero_introductions(manifest):
    """核对正式十二生肖集合及作者中文正文；空白/遗漏失败，绝不以默认介绍自动补齐。"""
    heroes = manifest.get("heroes", [])
    if len(heroes) != 12 or {h.get("heroId") for h in heroes} != {"Hero.Zodiac." + s for s in HERO_SUFFIXES}:
        raise ValueError("英雄IntroductionZh源须恰好覆盖既有十二生肖身份，不得重复或新增协议身份")
    for hero in heroes:
        text = hero.get("IntroductionZh")
        if not isinstance(text, str) or not text.strip():
            raise ValueError("英雄IntroductionZh缺失或纯空白：" + hero["heroId"])


def _require_fields(value, fields, label):
    """字段精确白名单阻断数字、技能集合或Gameplay软路径混入纯介绍文件。"""
    if not isinstance(value, dict) or set(value) != set(fields):
        raise ValueError(label + "不符合纯介绍字段白名单；不得复制数值、技能授权或其他Gameplay字段")


def _settings_mapping(settings_text):
    """读取本次明确采用的单行UE TMap文本；拒绝覆盖键/重复英雄/额外导入语法，非通用INI解析器。"""
    section, lines = "", []
    for raw in settings_text.splitlines():
        line = raw.strip()
        if not line or line.startswith((";", "#")):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
        elif section == SETTINGS_SECTION and "=" in line:
            key, value = line.split("=", 1)
            if key.lstrip("+-!.") == "CharacterCreationProfiles":
                lines.append((key, value))
    if len(lines) != 1 or lines[0][0] != "CharacterCreationProfiles":
        raise ValueError("客户端介绍软映射必须使用一个明确的CharacterCreationProfiles TMap，不得重复覆盖")
    value = re.sub(r"\s+", "", lines[0][1])
    pairs = list(re.finditer(r'\("([^"\r\n]+)","([^"\r\n]+)"\)', value))
    if value != "(" + ",".join(p.group(0) for p in pairs) + ")":
        raise ValueError("客户端介绍软映射TMap格式无效")
    mapping = {p.group(1): p.group(2) for p in pairs}
    if len(mapping) != len(pairs):
        raise ValueError("客户端介绍软映射存在重复英雄")
    return mapping


def validate_creation_sources(profiles, heroes, settings_text, draft):
    """核对已批准的源形状/身份及软资源边界；draft仅比对候选身份/名称/图标，不读取数值用于显示。"""
    validate_hero_introductions(heroes)
    _require_fields(profiles, ("_description", "SchemaVersion", "ProfileClassPath", "PrimaryAssetType", "DraftNoticeZh", "SourceSkillDraft", "Profiles"), "介绍源")
    if profiles["SchemaVersion"] != 1 or profiles["ProfileClassPath"] != "/Script/DivineBeastsUIClient.DivineBeastsCharacterCreationProfile" or profiles["PrimaryAssetType"] != "DivineBeastsCharacterCreationProfile":
        raise ValueError("创建介绍必须保留独立反射类和主资产类型，不能复制战斗Profile注册身份")
    if profiles["DraftNoticeZh"] != DRAFT_NOTICE or profiles["SourceSkillDraft"] != DRAFT_SOURCE:
        raise ValueError("介绍源未批准声明或开发身份溯源不匹配")
    if draft.get("Status") != "DevelopmentDraftNotApproved" or draft.get("ShippingEligible") is not False:
        raise ValueError("候选源必须保留开发草案边界；正式批准迁移需要独立评审")
    candidates = {(e["HeroDefinitionId"], e["Slot"]): e for e in draft["Entries"]}
    if len(candidates) != 60 or len(draft["Entries"]) != 60:
        raise ValueError("既有开发候选身份集合不完整或重复")
    items = profiles["Profiles"]
    if len(items) != 12 or len({p.get("HeroDefinitionId") for p in items}) != len(items):
        raise ValueError("创建介绍须为十二份，英雄身份不得重复")
    expected_ids = {"Hero.Zodiac." + s for s in HERO_SUFFIXES}
    if {p["HeroDefinitionId"] for p in items} != expected_ids:
        raise ValueError("创建介绍英雄集合与当前十二生肖不符")
    expected_mapping = {}
    for profile in items:
        _require_fields(profile, ("HeroDefinitionId", "AssetPath", "bDescriptionsAreDraft", "Entries"), "Profile")
        hero_id = profile["HeroDefinitionId"]
        suffix = hero_id.split(".")[-1]
        package = PROFILE_ROOT + "DA_DBA_CharacterCreation_" + suffix
        if profile["AssetPath"] != package:
            raise ValueError("Profile软映射必须指向唯一DBAUIPack_Core创建介绍包：" + hero_id)
        if profile["bDescriptionsAreDraft"] is not True:
            raise ValueError("开发候选介绍必须显式保持草案状态：" + hero_id)
        expected_mapping[hero_id] = package + "." + package.split("/")[-1]
        entries = profile["Entries"]
        if tuple(e.get("Slot") for e in entries) != SLOTS:
            raise ValueError("创建介绍五槽顺序必须为普攻、被动、主动一、主动二、终极：" + hero_id)
        for entry in entries:
            _require_fields(entry, ("Slot", "AbilityId", "DisplayNameZh", "DescriptionZh", "IconAssetPath"), "Entry")
            candidate = candidates.get((hero_id, entry["Slot"]))
            if not candidate or entry["AbilityId"] != candidate["DevelopmentAbilityId"] or not re.fullmatch(r"dba\.ability\." + suffix.lower() + r"_dev_[a-z0-9_]+@1", entry["AbilityId"]):
                raise ValueError("创建介绍技能身份必须是同英雄原始候选，不能变成授权ID：" + hero_id)
            if entry["DisplayNameZh"] != candidate["DisplayNameZh"] or not entry["DisplayNameZh"].strip():
                raise ValueError("候选中文名称须与作者身份源一致：" + hero_id)
            description = entry["DescriptionZh"]
            if not isinstance(description, str) or DRAFT_NOTICE not in description or not description.replace(DRAFT_NOTICE, "").strip("。 \t\r\n"):
                raise ValueError("每项正文须包含未批准声明及实际介绍，不能只有状态口号：" + hero_id)
            icon = entry["IconAssetPath"]
            if icon != candidate["IconAssetPath"] or not icon.startswith("/DBAHeroPack_" + suffix + "/UI/Abilities/"):
                raise ValueError("图标必须使用该英雄既有UI内容路径，不能引用开发Gameplay包：" + hero_id)
    if _settings_mapping(settings_text) != expected_mapping:
        raise ValueError("客户端介绍软映射必须与十二份纯介绍源严格一致，不能引入开发Profile/技能集合")


def load_creation_sources(workspace):
    """按调用方提供的根路径只读加载；路径不硬编码个人磁盘，失败原样报告、不写伪证据。"""
    root = Path(workspace)
    folder = root / "Tools/Unreal/Characters"
    return (json.loads((folder / "CharacterCreationProfiles.json").read_text(encoding="utf-8-sig")),
        json.loads((folder / "ZodiacPlaceholderCharacters.json").read_text(encoding="utf-8-sig")),
        (root / "Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig"),
        json.loads((root / DRAFT_SOURCE).read_text(encoding="utf-8-sig")))


if __name__ == "__main__":
    validate_creation_sources(*load_creation_sources(Path(__file__).resolve().parents[3]))
    print("创建介绍源与前端软映射静态合同一致；未检查真实UE资产、构建、Cook或客户端。")
