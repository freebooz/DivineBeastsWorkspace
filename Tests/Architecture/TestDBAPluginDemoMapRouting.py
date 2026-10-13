#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""插件开发门户的真实地图归属和路由安全静态回归。

这是源码契约检查，不代替Editor C++自动化、AssetRegistry加载或实际进入/返回地图。
防止所有Foundation审核场景被错误归入DeveloperTools，或多插件共享图因复用ScenarioId被混淆。
"""
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
CATALOG=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/Demo/DivineBeastsDemoCatalogSubsystem.cpp"
WORLD=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/Demo/DivineBeastsDemoWorldSubsystem.cpp"


class PluginDemoRoutingPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog=CATALOG.read_text(encoding="utf-8-sig")
        cls.world=WORLD.read_text(encoding="utf-8-sig")

    def test_core_review_categories_not_all_misassigned_to_developer_tools(self):
        for identity in ("GamePlatformCombat","GamePlatformAI","GamePlatformInteraction",
                         "GamePlatformNavigation","GamePlatformSFX","GamePlatformPCG",
                         "DBAArena","GamePlatformArena","DBAGameplay"):
            with self.subTest(plugin=identity):
                self.assertIn(f'TEXT("{identity}")',self.catalog)

    def test_hero_showcase_reusable_for_twelve_content_packs(self):
        self.assertIn("L_DBA_HeroShowcase",self.catalog)
        for name in ("Rat","Ox","Tiger","Rabbit","Dragon","Snake",
                     "Horse","Goat","Monkey","Rooster","Dog","Boar"):
            self.assertIn(f'TEXT("DBAHeroPack_{name}")',self.catalog)

    def test_multiple_plugins_get_unique_scenario_identity_without_dup_map(self):
        self.assertIn("ResolveScenarioPluginIds",self.catalog)
        self.assertIn("PluginId.ToString()",self.catalog)
        self.assertIn("Item.MapAsset = Asset.ToSoftObjectPath()",self.catalog)
        self.assertIn("Scenarios.Add(MoveTemp(Item))",self.catalog)
        self.assertIn("Item->MapAsset.GetLongPackageName()",self.catalog)

    def test_portal_itself_is_not_selectable_as_gameplay_demo(self):
        self.assertIn("FDivineBeastsPluginDemoMode::GetHubMapPackage()",self.catalog)
        self.assertIn("if (PackagePath == FDivineBeastsPluginDemoMode::GetHubMapPackage())",self.catalog)

    def test_world_switch_uses_real_asset_path_not_alias_id(self):
        self.assertIn("Scenario.MapAsset.GetLongPackageName()",self.world)
        self.assertIn("Scenario.bAvailable",self.world)


if __name__=="__main__":
    unittest.main(verbosity=2)
