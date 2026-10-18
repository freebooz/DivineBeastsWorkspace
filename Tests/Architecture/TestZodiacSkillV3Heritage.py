#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 十二生肖技能历史继承V3纯数据与创角预览审核门禁。

仅检查V2/V3 JSON、原有生肖图标清单、创角UI C++源码合同及审核状态。
不会编译UE、导入/修改.uasset，不把脚本通过误算技能实现或视觉签收。
"""
from __future__ import annotations

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
V2 = ROOT / "Docs/Design/ZodiacEightSlotSkills_V2.json"
V3 = ROOT / "Docs/Design/ZodiacSkillV3Review"
INDEX = V3 / "ReviewIndex.json"
PREVIEW = V3 / "CharacterCreatePreviewContract.json"
ART = ROOT / "Docs/Art/ZodiacSkillIconPrompts.json"
HEROES = ["Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
          "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar"]
SLOTS = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]
ROLE_ORDER = ["基础攻击", "攻击技能", "扩展攻击", "终极技能", "移动", "防御", "控制", "特色／治疗"]


def open_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


class ZodiacSkillV3HeritageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.v2 = open_json(V2)
        cls.index = open_json(INDEX)
        cls.preview = open_json(PREVIEW)
        cls.icons = open_json(ART)
        cls.heroes = {name: open_json(V3 / f"Hero_{name}.json") for name in HEROES}

    def test_01_12_heroes_and_96_slots_match_v2_ability_ids(self):
        self.assertEqual(len(self.heroes), 12)
        self.assertEqual(self.index["counts"]["hero"], 12)
        self.assertEqual(self.index["counts"]["skill"], 96)
        self.assertEqual(self.index["stableAbilityIdUniqueCount"], 96)
        all_ids = []
        for v2hero in self.v2["heroes"]:
            name = v2hero["heroId"].split(".")[-1]
            v3 = self.heroes[name]
            self.assertEqual(v3["heroId"], v2hero["heroId"])
            self.assertEqual(v3["slotCount"], 8)
            self.assertEqual([r["slotId"] for r in v3["skillRows"]], SLOTS)
            self.assertEqual([r["roleZh"] for r in v3["skillRows"]], ROLE_ORDER)
            self.assertEqual([r["abilityId"] for r in v3["skillRows"]],
                             [r["abilityId"] for r in v2hero["slots"]])
            all_ids.extend(r["abilityId"] for r in v3["skillRows"])
        self.assertEqual(len(all_ids), len(set(all_ids)))

    def test_02_historical_six_roles_retained_without_retired_gameplay(self):
        for hero in self.heroes.values():
            self.assertTrue(hero["bannedMechanicsNeverRestore"])
            self.assertEqual(sum(row["historicalSource"].get("legacySlot") is not None
                                 for row in hero["skillRows"]), 6)
            self.assertTrue(hero["passiveTrait"]["nameZh"])
            for row in hero["skillRows"]:
                self.assertTrue(row["nameZh"].strip())
                self.assertNotIn("王印", row["nameZh"])
                self.assertNotIn("王座", row["nameZh"])
                self.assertNotIn("三合", row["creatorDescriptionZh"])
                self.assertNotIn("相生克制", row["creatorDescriptionZh"])
                self.assertIn("本条为创角预览", row["creatorDescriptionZh"])

    def test_03_retired_king_seal_ultimate_labels_are_explicitly_migrated(self):
        for hero, old, new in (
                ("Rat", "万鼠盗王印", "万鼠影袭"),
                ("Horse", "雷蹄破王阵", "雷蹄破阵"),
                ("Boar", "玄鬃裂王座", "玄鬃裂地")):
            source = self.heroes[hero]
            self.assertEqual(source["oldUltimateName"], old)
            self.assertEqual(source["reviewUltimateName"], new)
            self.assertEqual(source["skillRows"][3]["nameZh"], new)

    def test_04_preserve_dragon_historical_six_skills(self):
        rows = self.heroes["Dragon"]["skillRows"]
        self.assertEqual([r["nameZh"] for r in rows],
                         ["苍龙爪光", "雷裁", "苍云雷息", "苍龙定天仪",
                          "御风", "云护", "龙压", "龙域"])
        self.assertEqual(self.heroes["Dragon"]["passiveTrait"]["nameZh"], "天象轮转")
        self.assertEqual(rows[1]["labelHistoricalV2Alias"], "云鳞飞刃")

    def test_05_v2_level1_candidate_stats_unchanged_not_authorized(self):
        for v2hero in self.v2["heroes"]:
            v3 = self.heroes[v2hero["heroId"].split(".")[-1]]
            for old, row in zip(v2hero["slots"], v3["skillRows"]):
                cand = row["balanceV2Reference"]
                for v3field,v2field in (
                    ("baseDamage", "baseDamage"),("cooldownSeconds","cooldownSeconds"),
                    ("momentumCost","momentumCost"),("energyCost","energyCost"),
                    ("rangeCm","castRangeCm"),("radiusCm","areaRadiusCm"),
                    ("hitCount","hitCount")):
                    self.assertEqual(cand[v3field], old[v2field])
                self.assertFalse(row["status"]["canShowInProductionCreation"])
                self.assertEqual(row["status"]["gameplayAbility"], "NOT_AUTHORED")
                self.assertIsNone(row["imageConcept"]["actualNiagaraAssetPath"])
                self.assertIsNone(row["imageConcept"]["proposedPreviewImagePath"])

    def test_06_60_icons_are_only_visual_references_not_approved_skill_images(self):
        self.assertEqual(len(self.icons["Icons"]), 60)
        paths = {x["TextureAssetPath"] for x in self.icons["Icons"]}
        for hero in self.heroes.values():
            self.assertIn(hero["passiveTrait"]["existingIcon"], paths)
            for row in hero["skillRows"]:
                icon = row["iconReference"]
                if icon is not None:
                    self.assertIn(icon["existingTexturePath"], paths)
                    self.assertIn("REFERENCE_ONLY", icon["status"])
                self.assertEqual(row["status"]["effectImage"], "NOT_GENERATED")
                self.assertEqual(len(row["imageConcept"]["keyframes"]), 3)
                self.assertIn("45度", row["imageConcept"]["shortPromptZh"])

    def test_07_creation_ui_needs_versioned_8_skill_and_image_channel(self):
        self.assertEqual(self.preview["status"], "DESIGN_REVIEW_ONLY_NOT_CONNECTED_OR_IMPORTED")
        self.assertEqual(self.preview["reviewLayout"]["cardCount"], 8)
        self.assertEqual(len(self.preview["heroDisplaySources"]), 12)
        self.assertEqual(self.preview["approvalGate"]["creationPreviewImage"], "NOT_IMPORTED")
        legacy = (ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/"
                  "Private/Definitions/DivineBeastsCharacterCreationProfile.cpp").read_text(encoding="utf-8-sig")
        self.assertIn("Entries.Num() != 5", legacy)
        self.assertEqual(self.preview["existingAssetsAndClasses"]["currentSkillLimit"], 5)
        self.assertFalse(self.preview["existingAssetsAndClasses"]["effectImageFieldExists"])

    def test_08_no_inadvertent_override_or_created_native_assets_claim(self):
        self.assertFalse(self.index["readyForCreationScreen"])
        self.assertEqual(self.index["importedNativeV3Assets"], 0)
        self.assertEqual(self.index["approvedEffectImageCount"], 0)
        self.assertTrue(all(not entry["visibleInProductionCreation"]
                            for entry in self.preview["heroDisplaySources"]))

    def test_09_all_12_historial_names_are_locked_to_review_matrix(self):
        # 96个展示名必须来源于遗产迁移表；允许在人工批准后形成新修订，禁止无记录随意改名。
        expected = {
                "Rat": [
                        "影牙斩",
                        "咬影",
                        "交锋裂影",
                        "万鼠影袭",
                        "钻缝",
                        "藏尾",
                        "咬筋",
                        "嗅踪"
                ],
                "Ox": [
                        "玄角撞",
                        "裂角",
                        "裂土重踏",
                        "不动玄门",
                        "沉步",
                        "镇山护壁",
                        "顶回",
                        "立阵"
                ],
                "Tiger": [
                        "白君轻爪",
                        "裂爪",
                        "猛虎连袭",
                        "白虎啸岭杀",
                        "扑杀",
                        "虎踞",
                        "虎啸",
                        "开猎"
                ],
                "Rabbit": [
                        "月玉轻弹",
                        "玉刃",
                        "落英星雨",
                        "千月回环",
                        "踏月",
                        "回月",
                        "月痕",
                        "玉露回春"
                ],
                "Dragon": [
                        "苍龙爪光",
                        "雷裁",
                        "苍云雷息",
                        "苍龙定天仪",
                        "御风",
                        "云护",
                        "龙压",
                        "龙域"
                ],
                "Snake": [
                        "幽鳞穿刺",
                        "毒牙",
                        "盘鳞双弹",
                        "万毒归鳞",
                        "蛇行",
                        "蜕鳞",
                        "缠纹",
                        "封途"
                ],
                "Horse": [
                        "雷蹄踢",
                        "雷枪",
                        "惊雷践踏",
                        "雷蹄破阵",
                        "奔雷",
                        "踏阵",
                        "蹄震",
                        "先至"
                ],
                "Goat": [
                        "玉角光刺",
                        "净光",
                        "玉符环阵",
                        "万愿归铃",
                        "铃步",
                        "守愿",
                        "安魂",
                        "铃印"
                ],
                "Monkey": [
                        "灵棍点打",
                        "戏棍",
                        "云棍横扫",
                        "万象戏台",
                        "筋斗",
                        "替身",
                        "扰心",
                        "百戏"
                ],
                "Rooster": [
                        "金羽啄光",
                        "破晓",
                        "晨鸣音浪",
                        "破晓金鸣",
                        "鸣步",
                        "金羽",
                        "鸣震",
                        "晨鸣"
                ],
                "Dog": [
                        "天犬咬击",
                        "反咬",
                        "灵爪连击",
                        "天门守阙",
                        "援步",
                        "护主",
                        "拦门",
                        "誓约"
                ],
                "Boar": [
                        "玄鬃冲撞",
                        "怒锤",
                        "重压冲波",
                        "玄鬃裂地",
                        "獠撞",
                        "岩甲",
                        "震鬃",
                        "鬃吼"
                ]
        }
        for hero, names in expected.items():
            self.assertEqual([row["nameZh"] for row in self.heroes[hero]["skillRows"]], names)

    def test_10_heal_capable_heroes_preserve_v2_safety_roles(self):
        healers = set()
        for hero, obj in self.heroes.items():
            if any(any(action["kind"] == "healing" for action in row["balanceV2Reference"]["actions"])
                   for row in obj["skillRows"]):
                healers.add(hero)
        self.assertEqual(healers, {"Rabbit", "Goat", "Rooster", "Dog"})
        self.assertTrue(all(not row["status"]["canShowInProductionCreation"]
                            for obj in self.heroes.values() for row in obj["skillRows"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
