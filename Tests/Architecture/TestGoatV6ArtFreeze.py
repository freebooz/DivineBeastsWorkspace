# -*- coding: utf-8 -*-
"""未羊V6八技能图标冻结合同的跨引擎静态回归。

所属：SSLM Tests/Architecture（测试与架构门禁）。本测试只验证技能身份、
V5/V6名称、来源Hash的结构与客户端内容包目录所有权。
不生成假纹理、不修改游戏、不把静态成功声明为原生资产或HUD已通过。
运行：python Tests/Architecture/TestGoatV6ArtFreeze.py
"""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GOAT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat"
SLOT_ORDER = ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"]


class GoatV6ArtFreezeTest(unittest.TestCase):
    """只读地校验单英雄八槽冻结、纹理签名及未批准玩法边界。"""

    @classmethod
    def setUpClass(cls):
        """读取真实工程中的V6审核稿，避免独立复制第二套技能真源。"""
        cls.art = json.loads(
            (GOAT / "Docs/GoatV6_ArtFreeze_20261020.json").read_text(encoding="utf-8"))
        cls.design = json.loads(
            (ROOT / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json")
            .read_text(encoding="utf-8"))
        cls.goat = next(h for h in cls.design["heroes"]
                        if h["heroId"] == "Hero.Zodiac.Goat")

    def test_eight_slots_stable_identity(self):
        """防止未羊技能错槽、改稳定编号或与别的生肖混用。"""
        self.assertEqual("Hero.Zodiac.Goat", self.art["heroId"])
        self.assertEqual(SLOT_ORDER, self.art["slotOrder"])
        rows = self.art["skills"]
        self.assertEqual(8, len(rows))
        self.assertEqual(SLOT_ORDER, [x["slotId"] for x in rows])
        ids = [x["abilityId"] for x in rows]
        self.assertEqual(8, len(set(ids)))
        self.assertTrue(all(x.startswith("dba.ability.goat_eight_") and x.endswith("@1")
                            for x in ids))

    def test_names_and_effects_from_v6(self):
        """图标设计可采用V6候选，但正式显示名称保持现有V5审批状态。"""
        for actual, planned in zip(self.art["skills"], self.goat["skills"]):
            self.assertEqual(planned["abilityId"], actual["abilityId"])
            self.assertEqual(planned["proposedNameZh"], actual["v6DisplayNameZh"])
            self.assertEqual(planned["currentApprovedV5NameZh"], actual["productionV5NameZh"])
            self.assertEqual(planned["descriptionZh"], actual["descriptionZh"])
        self.assertTrue(self.art["productionV5NamesUnchanged"])
        self.assertEqual("NOT_GRANTED", self.art["gasAuthorization"])
        self.assertEqual("PENDING_HUMAN_REVIEW", self.art["artApproval"])

    def test_icon_locations_hashes_and_dimensions_contract(self):
        """PNG路径只能位于羊专属内容包，原图、审核和运行图必须分别标识。"""
        files = set()
        icons = set()
        prefix = "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Goat/"
        for row in self.art["skills"]:
            self.assertTrue(row["texturePath"].startswith(
                "/DBAHeroPack_Goat/UI/Abilities/V6/Textures/T_DBA_Goat_V6_"))
            self.assertNotIn(row["texturePath"], icons)
            icons.add(row["texturePath"])
            for key, fragment in (("sourceFile","/Original/"),
                                  ("png64","/Review64/"),("png48","/Runtime48/")):
                name = row[key]
                self.assertTrue(name.startswith(prefix))
                self.assertIn(fragment, name)
                self.assertTrue(name.endswith(".png"))
                self.assertNotIn(name, files)
                files.add(name)
            for sha in row["sha256"].values():
                self.assertTrue(re.fullmatch(r"[0-9a-f]{64}", sha))
        self.assertEqual(24, len(files))

    def test_no_passive_slot_or_gameplay_authorization(self):
        """八槽以外的被动灵愿不应通过UI图标接线自动授予。"""
        self.assertEqual("灵愿", self.art["passive"]["nameZh"])
        self.assertFalse(self.art["passive"]["occupiesSlot"])
        self.assertEqual("V6_ART_ONLY", self.art["frozenScope"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
