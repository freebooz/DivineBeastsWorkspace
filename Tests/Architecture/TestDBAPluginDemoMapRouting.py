#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""插件演示目录的稳定身份、资源路由和失败关闭回归（纯源码/登记数据审查）。

真实项目的正式演示已迁移到中央Data定义、快照发布、适配器预检；开发地图只能作为
未编目诊断候选，不得通过名称推测形成已完成的插件功能。只检查合同文本和真实清单，
不代替C++链接、UE AssetRegistry真实加载、场景导航或人工视觉验收。
"""
from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient"
CATALOG = BASE / "Private/Demo/DivineBeastsDemoCatalogSubsystem.cpp"
WORLD = BASE / "Private/Demo/DivineBeastsDemoWorldSubsystem.cpp"
MANIFEST = ROOT / "Docs/Implementation/PluginDemos/PluginDemoManifest.json"


class PluginDemoRoutingPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.catalog = CATALOG.read_text(encoding="utf-8-sig")
        cls.world = WORLD.read_text(encoding="utf-8-sig")
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))

    def test_discovery_uses_real_plugin_descriptors_not_hardcoded_ownership(self):
        # 只能列出实际项目插件，不允许基于开发地图路径伪造正式功能归属。
        self.assertIn("IPluginManager::Get().GetDiscoveredPlugins()", self.catalog)
        self.assertIn("Plugin->IsEnabled()", self.catalog)
        self.assertIn("ResolveLayer(", self.catalog)
        self.assertNotIn("GuessOwningPlugin(", self.catalog)
        self.assertNotIn("ResolveScenarioPluginIds(", self.catalog)

    def test_unreviewed_hero_packs_are_not_automatically_counted_complete(self):
        # 12生肖内容包可以显示为未审查，不能因为有共享地图便推定12个英雄功能均完成。
        actual = {item["plugin_id"]: item for item in self.manifest["plugins"]}
        for name in ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
                     "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar"):
            with self.subTest(hero=name):
                record = actual["DBAHeroPack_" + name]
                self.assertEqual(record["feature_audit_status"], "unreviewed")
                self.assertTrue((ROOT / record["descriptor_path"]).is_file())

    def test_formal_scenario_uses_stable_definition_id_and_asset(self):
        # 正式身份来自定义，而不是同名地图或字符串拼接。
        self.assertIn("PrivateState->Snapshot.Publish(*Definition, Plugins)", self.catalog)
        self.assertIn("Item.ScenarioId = Definition.ScenarioId", self.catalog)
        self.assertIn("Item.MapAsset = Definition.MapAsset", self.catalog)
        self.assertIn("PrivateState->DiagnosticMaps.Add(MoveTemp(Item))", self.catalog)
        self.assertIn("RegisteredMaps.Contains(", self.catalog)
        self.assertIn("PreflightScenario(Item.ScenarioId)", self.catalog)

    def test_hub_map_never_presented_as_selectable_diagnostic_scenario(self):
        # 门户自身被排除；其余仅允许开发目录内真实World资产。
        self.assertIn('Registry.GetAssetsByPath(FName(TEXT("/Game/Development"))', self.catalog)
        self.assertIn("Package == FDivineBeastsPluginDemoMode::GetHubMapPackage()", self.catalog)
        self.assertIn('Package.StartsWith(TEXT("/Game/Development/"))', self.catalog)
        self.assertIn("Asset.AssetClassPath != UWorld::StaticClass()->GetClassPathName()", self.catalog)

    def test_formal_open_requires_identity_resolution_and_preflight(self):
        # 地图资源只通过已登记软路径切换，先作真正适配器/目标/插件/资产检查。
        self.assertIn("PrivateState->Snapshot.ResolveIdentity(InputId, Resolved)", self.catalog)
        self.assertIn("PreflightScenario(Item->ScenarioId)", self.catalog)
        self.assertIn("!FPackageName::DoesPackageExist(", self.catalog)
        self.assertIn("PrivateState->PendingMap = ", self.catalog)
        self.assertIn("UGameplayStatics::OpenLevel(GetWorld(), Map);", self.catalog)

    def test_world_switch_checks_actual_asset_path(self):
        self.assertIn("FName(*Scenario.MapAsset.GetLongPackageName()) == CurrentMap", self.world)
        self.assertNotIn("Scenario.ScenarioId == CurrentMap", self.world)
        self.assertIn("RequestDemoOverlay(InWorld)", self.world)



if __name__=="__main__":
    unittest.main(verbosity=2)
