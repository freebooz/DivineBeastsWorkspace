#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
《神兽联盟》十二生肖技能交付预检的无引擎单元测试。

职责：检查平台逻辑身份合法性、12生肖资产清单真实路径、授权/界面/服务端缺项的
      fail-closed（失败即拒绝）结果。使用内存副本作为负向夹具，不写入生产资产。
端侧：开发与持续集成测试工具；不替代UE5.8/Monolith/GAS联网与Cook。
用法：python Tests/Assets/TestZodiacAbilityDelivery.py
退出码：0=本套脚本断言通过，非0=失败。
"""
import copy
import importlib.util
import json
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).resolve().with_name("ValidateZodiacAbilityDelivery.py")
SPEC = importlib.util.spec_from_file_location("zodiac_ability_delivery", MODULE_PATH)
DELIVERY = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(DELIVERY)


class ZodiacAbilityDeliveryTests(unittest.TestCase):
    """资源存在不能等于生产可用；测试负向条件是否准确拒绝。"""

    @classmethod
    def setUpClass(cls):
        """真实只读基础清单，本套测试不产生任何假技能二进制资产。"""
        cls.inventory = json.loads(DELIVERY.INVENTORY_PATH.read_text(encoding="utf-8"))

    def test_single_namespace_identity_accepted(self):
        """平台FGamePlatformId允许一个namespace+name，不能要求三段。"""
        self.assertIsNotNone(DELIVERY.DEFAULT_ID_RE.fullmatch("dba.abilityset@1"))
        self.assertIsNotNone(DELIVERY.DEFAULT_ID_RE.fullmatch("dba.hero.rat@3"))

    def test_invalid_identity_rejected(self):
        """拒绝空命名空间、非法字符、版本0及非规范前导零。"""
        for bad in ("foo@1", "foo.bar@0", "foo.bar@01", "foo..bar@1",
                    "foo.b@-1", "foo/bar@1", "foo.bar@x", "中文.技能@1"):
            with self.subTest(value=bad):
                self.assertIsNone(DELIVERY.DEFAULT_ID_RE.fullmatch(bad))

    def test_twelve_heroes_have_real_assets(self):
        """清单核查12名真实生肖英雄和60张Texture2D，并确认不存在伪蓝图。"""
        self.assertEqual(DELIVERY.require_files(self.inventory), [])

    def test_release_rejects_missing_formal_skillsets(self):
        """美术图标齐全不等于服务器AbilitySet已授权。"""
        missing = DELIVERY.unmet_release_requirements(self.inventory)
        self.assertEqual(len(missing), 70)
        self.assertTrue(any("Hero.Zodiac.Rat: 真实DefaultAbilitySetId" in s for s in missing))
        self.assertTrue(any("专用服务器" in s for s in missing))
        self.assertTrue(any("服务器构建" in s for s in missing))

    def test_missing_icon_causes_inventory_failure(self):
        """虚构的每英雄已导入数量不能让缺陷清单通过。"""
        fixture = copy.deepcopy(self.inventory)
        fixture["Heroes"][0]["ImportedArtSlotTexturesVerified"] = 0
        self.assertTrue(any("Rat" in x for x in DELIVERY.require_files(fixture)))
        self.assertEqual(DELIVERY.require_files(self.inventory), [])

    def test_invalid_count_causes_inventory_failure(self):
        """清单缺英雄时必须失败，不允许默默跳过。"""
        fixture = copy.deepcopy(self.inventory)
        fixture["Heroes"].pop()
        self.assertTrue(DELIVERY.require_files(fixture))

    def test_approved_default_id_reduces_only_its_one_blocker(self):
        """在内存中填入单个合法集合ID也不能直接解除其余资格。"""
        fixture = copy.deepcopy(self.inventory)
        fixture["Heroes"][0]["DefaultAbilitySetId"] = "dba.rat_abilities@1"
        before = DELIVERY.unmet_release_requirements(self.inventory)
        after = DELIVERY.unmet_release_requirements(fixture)
        self.assertEqual(len(after), len(before) - 1)
        self.assertTrue(any("Hero.Zodiac.Rat: 缺少专用服务器" in s for s in after))

    def test_full_release_requires_real_network_evidence(self):
        """没有真实联机与烘焙证明时，静态资产清单不得被误判为发布合格。"""
        fields = ["UEEditorClientServerBuildVerified",
                  "GameplayAutomationVerified",
                  "TwoClientDedicatedServerVerified",
                  "ClientCookVerified",
                  "ServerCookExclusionVerified"]
        missing = DELIVERY.unmet_release_requirements(self.inventory)
        for field in fields:
            with self.subTest(field=field):
                self.assertTrue(any(field in s for s in missing))


if __name__ == "__main__":
    unittest.main(verbosity=2)
