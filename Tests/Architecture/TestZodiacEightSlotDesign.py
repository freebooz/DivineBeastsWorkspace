#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM十二生肖八技能设计／输入／DataAsset制作源静态门禁。

只读取JSON、CSV、C++与Ini；不会导入/修改.uasset，不触发UHT/UBT或运行编辑器。
96份设计草案不等同于96个已执行GameplayAbility。
"""
from __future__ import annotations
import copy
import csv
import io
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CATALOG_PATH = ROOT / "Docs/Design/ZodiacEightSlotSkills_V2.json"
BALANCE_PATH = ROOT / ("Game/Plugins/DivineBeasts/DBAGameplay/SourceArt/Abilities/"
                        "DT_DBA_ZodiacEightSlotDraftBalance_V2.csv")
GAME = ROOT / "Game"
EXPECTED_HEROES = {
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar"
}
SLOTS = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]
ROLES = ["basic", "attack_a", "attack_b", "ultimate", "mobility", "defense", "control", "signature"]
HEALER_CODES = {"rabbit", "goat", "rooster", "dog"}
KINDS = {"damage", "movement", "shield", "healing", "control", "buff"}
ENGINES = {"Physical", "Magic", "Untyped"}


def source(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8-sig")


def load() -> dict:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8-sig"))


def all_skills(catalog: dict) -> list[dict]:
    return [row for hero in catalog["heroes"] for row in hero["slots"]]


def balanced_csv() -> dict[str, dict]:
    rows = list(csv.DictReader(BALANCE_PATH.open(encoding="utf-8-sig", newline="")))
    return {row["AbilityId"]: row for row in rows}


def audit(catalog: dict) -> list[str]:
    errors = []
    def fail(ok, message):
        if not ok:
            errors.append(message)

    fail(catalog.get("schemaVersion") == 2, "结构版本非法")
    fail(catalog.get("nativeUEAssetStatus") == "NOT_AUTHORED", "不能在未制作前标记真实.uasset完成")
    fail(catalog.get("designerApprovalStatus") == "PENDING_HUMAN_BALANCE_REVIEW", "禁止冒充数值正式获批")
    fail(catalog["slotPolicy"]["maxActiveSlots"] == 8, "不是八个可装载主动技能")
    fail(not catalog["slotPolicy"]["innatePassiveOccupiesSlot"], "固有被动不能挤占八技能槽")
    heroes = catalog["heroes"]
    fail(len(heroes) == 12, "不是12位生肖")
    fail({x["heroId"].rsplit(".", 1)[-1] for x in heroes} == EXPECTED_HEROES, "生肖目录不完整")
    all_ids = set()
    for hero in heroes:
        code = hero["heroCode"]
        rows = hero["slots"]
        fail(len(rows) == 8, f"{code}:槽数不为8")
        fail([x["slotId"] for x in rows] == SLOTS, f"{code}:显示槽顺序变更")
        fail([x["role"] for x in rows] == ROLES, f"{code}:攻防移动控制分类不完整")
        fail(bool(hero["innateTrait"]["nameZh"]) and
             hero["innateTrait"]["slotOccupancy"] == 0, f"{code}:独立固有被动不存在")
        fail(hero["proposedAbilitySetId"] == f"dba.abilityset.{code}_eight_draft@1",
             f"{code}:新版技能集草案ID不一致")
        for row in rows:
            identity = row["abilityId"]
            fail(re.fullmatch(r"dba\.ability\.[a-z0-9_]+@1", identity) is not None,
                 f"{code}:技能逻辑ID格式非法:{identity}")
            fail(identity.startswith(f"dba.ability.{code}_eight_"), f"{code}:技能ID跨英雄")
            fail(identity not in all_ids, f"{code}:重复ID")
            all_ids.add(identity)
            fail(not row["approvedForLiveGAS"] and
                 row["implementationStatus"] == "DESIGN_ONLY_UNIMPLEMENTED",
                 f"{identity}:没有实现不得授予")
            fail(row["level"] == 1, f"{identity}:必须只使用可验证的一级数值")
            fail(row["damageType"] in ENGINES, f"{identity}:伤害类型未知")
            fail(row["baseDamage"] >= 0 and 0 <= row["cooldownSeconds"] <= 120 and
                 row["energyCost"] == 0 and 0 <= row["momentumCost"] <= 100,
                 f"{identity}:数值越界或虚构蓝色能量成本")
            fail(row["damageType"] != "Magic" or row["momentumCost"] == 0,
                 f"{identity}:法术不得扣气势")
            fail(row["vfx"]["heroProfileId"] ==
                 "Presentation.VFX.Hero.Zodiac." + hero["heroId"].split(".")[-1] + ".Default",
                 f"{identity}:生肖VFX Profile串用")
            fail(row["vfx"]["assetPath"] is None and row["icon"]["assetPath"] is None,
                 f"{identity}:未制作VFX/UI不应伪造资产路径")
            actions = row["actions"]
            fail(bool(actions), f"{identity}:没有真实设计行为")
            fail(all(a["kind"] in KINDS for a in actions), f"{identity}:包含未知行为")
            fail(row["role"] != "mobility" or any(a["kind"] == "movement" for a in actions),
                 f"{identity}:移动槽没有合法位移")
            fail(row["role"] != "defense" or any(a["kind"] == "shield" for a in actions),
                 f"{identity}:防御槽没有护盾")
            fail(row["role"] != "control" or any(a["kind"] == "control" for a in actions),
                 f"{identity}:控制槽没有控制")
            fail(row["role"] != "basic" or row["baseDamage"] > 0, f"{identity}:普攻不能是治疗")
    skills = all_skills(catalog)
    fail(len(skills) == 96 and len(all_ids) == 96, "96技能数量或唯一性不满足")
    healers = {hero["heroCode"] for hero in heroes
               if any(a["kind"] == "healing" for row in hero["slots"] for a in row["actions"])}
    fail(healers == HEALER_CODES, "只有卯兔、未羊、酉鸡、戌狗具有明确治疗职责")
    return errors


class ZodiacEightSlotDraftTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = load()

    def test_01_full_catalog_contract(self):
        self.assertEqual(audit(self.catalog), [])

    def test_02_96_rows_match_csv(self):
        rows = list(csv.DictReader(BALANCE_PATH.open(encoding="utf-8-sig", newline="")))
        self.assertEqual(len(rows), 96)
        self.assertEqual(len({r["AbilityId"] for r in rows}), 96)
        for row in all_skills(self.catalog):
            record = next(r for r in rows if r["AbilityId"] == row["abilityId"])
            self.assertEqual(record["Name"], row["abilityId"].rsplit(".", 1)[-1].removesuffix("@1") + "_L1")
            for key, expected in (
                ("BaseDamage", row["baseDamage"]), ("CooldownSeconds", row["cooldownSeconds"]),
                ("MomentumCost", row["momentumCost"]), ("CastRangeCm", row["castRangeCm"]),
                ("AreaRadiusCm", row["areaRadiusCm"])
            ):
                self.assertAlmostEqual(float(record[key]), float(expected))
            self.assertEqual(record["DamageType"], row["damageType"])

    def test_03_8_slot_input_and_visible_widget_fields(self):
        input_source = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsInputClient/"
                              "Private/Input/DivineBeastsInputSemantics.cpp")
        panel = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/"
                       "Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp")
        panel_header = source("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/"
                              "Public/Panels/Combat/DivineBeastsAbilityBarPanel.h")
        tags = source("Game/Config/DefaultGameplayTags.ini")
        input_ini = source("Game/Config/DefaultInput.ini")
        for slot in self.catalog["slotPolicy"]["inputSemantics"]:
            self.assertIn(slot["tag"], tags)
            self.assertIn('ActionName="' + slot["action"] + '"', input_ini)
            self.assertIn(slot["tag"], panel)
            self.assertIn(slot["tag"], input_source if slot["slot"] in ("Primary", "Slot1",
                                                           "Slot2", "Slot3", "Slot4", "Slot5", "Slot6", "Slot7")
                          else panel)
        for name in ("MovementAbilitySlot", "DefenseAbilitySlot",
                     "ControlAbilitySlot", "SignatureAbilitySlot"):
            self.assertIn(name, panel_header)
            self.assertIn(name + ".Get()", panel)
        self.assertIn("DistinctActiveInputSlots.Num() == 8", panel)

    def test_04_draft_assets_fail_closed(self):
        definition = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                            "DivineBeastsAbilitiesRuntime/Public/Definitions/DivineBeastsAbilityDefinition.h")
        grant = source("Game/Plugins/DivineBeasts/DBAGameplay/Source/"
                       "DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp")
        author = source("Tools/Unreal/Abilities/AuthorZodiacEightSlotDesignAssets.py")
        self.assertIn("bEightSlotDraftOnly = false", definition)
        self.assertIn("if (Definition->bEightSlotDraftOnly)", grant)
        self.assertIn("unreal.DataTableFactory", author)
        self.assertIn("unreal.DataAssetFactory", author)
        self.assertIn("not row[\"approvedForLiveGAS\"]", author)
        self.assertNotIn("compile_blueprint(", author)
        self.assertNotIn("set_editor_property(\"DefaultAbilitySetId\"", author)

    def test_05_reject_missing_skill(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"].pop()
        self.assertNotEqual(audit(altered), [])

    def test_06_reject_duplicate_ability_id(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][1]["slots"][0]["abilityId"] = altered["heroes"][0]["slots"][0]["abilityId"]
        self.assertNotEqual(audit(altered), [])

    def test_07_reject_false_live_approval(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][0]["approvedForLiveGAS"] = True
        self.assertNotEqual(audit(altered), [])

    def test_08_reject_stealth_ability_without_movement(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][0]["slots"][4]["actions"] = [{"kind": "buff"}]
        self.assertNotEqual(audit(altered), [])

    def test_09_reject_missing_healing_capability(self):
        altered = copy.deepcopy(self.catalog)
        altered["heroes"][3]["slots"][7]["actions"] = [{"kind": "buff"}]
        altered["heroes"][3]["slots"][3]["actions"] = [{"kind": "damage"}]
        self.assertNotEqual(audit(altered), [])

    def test_10_preserves_legacy_primary_and_ultimate_tags(self):
        semantics = self.catalog["slotPolicy"]["inputSemantics"]
        self.assertEqual(semantics[0]["tag"], "Platform.Ability.Input.DivineBeasts.Primary")
        self.assertEqual(semantics[3]["tag"], "Platform.Ability.Input.DivineBeasts.Slot4")
        self.assertEqual([x["keyboard"] for x in semantics], list("12345678"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
