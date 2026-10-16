# -*- coding: utf-8 -*-
"""开发技能作者输入与发行隔离回归；离线执行不创建UE资产，不代表Cook或网络验证通过。"""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("development_author", ROOT / "Tools/Unreal/Abilities/AuthorZodiacDevelopmentAbilities.py")
AUTHOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUTHOR)


class DevelopmentAuthoringTests(unittest.TestCase):
    """校验候选真源、未实现机制禁用范围及双端开发配置的同一身份映射。"""

    def test_real_candidates_are_unique_and_unsupported_are_not_damage(self):
        """60项中只有41项可复用伤害样板；不把19项被动/辅助主题算成已实现伤害。"""
        entries = [entry for group in AUTHOR.load_plan().values() for entry in group]
        self.assertEqual(len(entries), 60)
        self.assertEqual(len({entry["DevelopmentAbilityId"] for entry in entries}), 60)
        self.assertEqual(sum(AUTHOR.is_damage_sample(entry) for entry in entries), 41)
        self.assertFalse(any(AUTHOR.is_damage_sample(entry) for entry in entries if entry["Slot"] == "Passive"))

    def test_village_uses_current_hero_default_and_default_is_closed(self):
        """Village联调沿用已批准占位技能，不能被旧候选集覆盖；正式默认仍排除开发目录。"""
        base = (ROOT / "Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
        self.assertIn("bAllowDevelopmentAbilitySets=false", base)
        self.assertIn('+DirectoriesToNeverCook=(Path="/Game/Development/DivineBeasts/Abilities")', base)
        for target in ("VillageDevelopmentClient", "VillageDevelopmentServer"):
            text = (ROOT / ("Game/Config/Custom/" + target + "/DefaultGame.ini")).read_text(encoding="utf-8-sig")
            # 保留双重许可供开发视觉和显式测试使用，但不再暗中把数字1治疗替换成旧普攻。
            self.assertIn("bAllowDevelopmentAbilitySets=true", text)
            for hero in AUTHOR.load_plan():
                self.assertNotIn("Hero.Zodiac." + hero + "=dba.abilityset." + hero.lower() + "_dev@1", text)
        server = (ROOT / "Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        self.assertIn('+DirectoriesToNeverCook=(Path="/Game/Development/DivineBeasts/Abilities/UI")', server)
        self.assertNotIn("AssetBaseClass=/Script/DivineBeastsUIClient", server)

    def test_village_ui_scans_profiles_for_the_granted_placeholder_abilities(self):
        """技能栏按Hero主资产ID查Profile；只扫描当前占位Profile，防止同Hero旧候选身份抢占。"""
        client = (ROOT / "Game/Config/Custom/VillageDevelopmentClient/DefaultGame.ini").read_text(encoding="utf-8-sig")
        scans = [line for line in client.splitlines() if line.startswith("+PrimaryAssetTypesToScan=") and
                 'PrimaryAssetType="DivineBeastsAbilityUIProfile"' in line]
        self.assertEqual(len(scans), 1)
        self.assertNotIn("/Game/Development/DivineBeasts/Abilities/UI/Profiles", scans[0])
        for hero in AUTHOR.load_plan():
            self.assertIn('/DBAHeroPack_' + hero + '/UI/Abilities', scans[0])

    def test_client_and_server_cooldown_dictionaries_match_candidate_duration(self):
        """同技能冷却字典必须双端一致；全部正数冷却候选必须具有独立标签。"""
        client = (ROOT / "Game/Config/Custom/VillageDevelopmentClient/DefaultGameplayTags.ini").read_text(encoding="utf-8-sig")
        server = (ROOT / "Game/Config/Custom/VillageDevelopmentServer/DefaultGameplayTags.ini").read_text(encoding="utf-8-sig")
        self.assertEqual(client, server)
        for hero, entries in AUTHOR.load_plan().items():
            for entry in entries:
                name = 'Tag="DivineBeasts.Development.Cooldown.' + hero + "." + entry["Slot"] + '"'
                self.assertEqual(name in client, entry["Level1BalanceDraft"]["CooldownSeconds"] > 0)


if __name__ == "__main__":
    unittest.main()
