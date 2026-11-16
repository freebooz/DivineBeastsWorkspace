#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM插件总览人工诊断地图入口的源代码与实际包头独立回归。

该检查只核对已有UE原生地图和UI资产、目录/菜单路由与不虚构人工批准的静态合同；
不启动编辑器，不制作Widget或地图，也不能替代Slate输入、蓝图加载和人工点击验收。
"""
from __future__ import annotations

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEMO = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient"
CORE = DEMO / "Private/Demo/DivineBeastsDemoCatalogSubsystem.cpp"
MODEL = DEMO / "Private/UI/DivineBeastsDemoHubViewModel.cpp"
SCREEN = DEMO / "Private/UI/DivineBeastsDemoHubScreen.cpp"
OVERLAY = DEMO / "Private/UI/DivineBeastsDemoHubWidget.cpp"
WORLD = DEMO / "Private/Demo/DivineBeastsDemoWorldSubsystem.cpp"
MAGIC = bytes.fromhex("c1 83 2a 9e")


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


class PluginVisualDiagnosticRoutingTests(unittest.TestCase):
    def test_real_editor_packages_not_fake_source_files(self):
        paths = (
            ROOT / "Game/Content/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.umap",
            ROOT / "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub.uasset",
            ROOT / "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoOverlay.uasset",
            ROOT / "Game/Content/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1.umap",
            ROOT / "Game/Content/Development/Weather/Maps/L_DBA_WeatherReview.umap",
        )
        for path in paths:
            with self.subTest(path=path.name):
                self.assertTrue(path.is_file(), "诊断入口只链接真实现存UE资产")
                with path.open("rb") as source:
                    self.assertEqual(MAGIC, source.read(4), "不是合法UE包头")
                self.assertGreater(path.stat().st_size, 64)

    def test_native_directory_never_promotes_diagnostic_map_to_formal_scenario(self):
        catalog = read(CORE)
        self.assertIn('Registry.GetAssetsByPath(FName(TEXT("/Game/Development"))', catalog)
        self.assertIn('Item.bDiagnosticOnly = true', catalog)
        self.assertIn('Item.PluginId = TEXT("GamePlatformDeveloperTools")', catalog)
        self.assertIn("if (!RegisteredMaps.Contains", catalog)
        self.assertIn("if (Resolution.Code == FName(TEXT(", catalog)
        self.assertIn("Item->bDiagnosticOnly", catalog)

    def test_modern_hub_shows_real_maps_without_creating_forged_features(self):
        model = read(MODEL)
        self.assertIn("#if WITH_EDITOR", model)
        self.assertIn('Snapshot.SelectedPluginId != TEXT("GamePlatformDeveloperTools")', model)
        self.assertIn('IsKnownDiagnosticMapForPlugin(Snapshot.SelectedPluginId, Package)', model)
        self.assertIn('Candidate.bDiagnosticOnly', model)
        self.assertIn('Candidate.MapAsset.IsNull()', model)
        self.assertIn('Row.Preflight.bCanStart = Candidate.bAvailable;', model)
        # 不在每次筛选时做磁盘IO；正式Catalog.OpenScenario仍执行实际包存在性校验。
        self.assertIn('FPackageName::DoesPackageExist(Item->MapAsset.GetLongPackageName())', read(CORE))
        self.assertIn('Row.Scenario.TemplateId = TEXT("DiagnosticMap")', model)
        self.assertIn("Snapshot.Scenarios.Add(MoveTemp(Row));", model)
        self.assertNotIn("Snapshot.KnownFeatures += Catalog->GetScenarios", model)
        self.assertNotIn("Snapshot.HumanApprovedFeatures +=", model)
        self.assertIn('return Catalog->OpenScenario(Id);', model)

    def test_domain_shortcuts_point_only_to_real_diagnostic_packages(self):
        model = read(MODEL)
        packages = (
            "/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1",
            "/Game/Development/Water/Maps/L_GP_WaterReview",
            "/Game/Development/Weather/Maps/L_DBA_WeatherReview",
            "/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview",
            "/Game/Development/Foundation/PluginReviews/Maps/L_GP_CombatReview",
            "/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview",
            "/Game/Development/Foundation/PluginReviews/Maps/L_GP_InteractionAIReview",
            "/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview",
            "/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview",
            "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase",
            "/Game/Development/Foundation/PluginReviews/Maps/L_DBA_MainArenaReview",
            "/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview",
        )
        self.assertEqual(12, len(set(packages)))
        for package in packages:
            with self.subTest(package=package):
                self.assertIn('Package == TEXT("' + package + '")', model)
                path = ROOT / (package.replace("/Game/", "Game/Content/", 1).lstrip("/") + ".umap")
                self.assertTrue(path.is_file(), "禁止将不存在的地图声明为可视化入口")
                with path.open("rb") as source:
                    self.assertEqual(MAGIC, source.read(4))
        self.assertIn('PluginId == TEXT("GamePlatformAI")', model)
        self.assertIn('PluginId == TEXT("GamePlatformInteraction")', model)
        for plugin in ("GamePlatformWorld", "DBAWorlds", "GamePlatformSurface",
                       "GamePlatformAnimation", "GamePlatformCharacter", "DBAGameplay",
                       "GamePlatformArena", "DBAArena", "DBAClient"):
            self.assertIn('PluginId == TEXT("' + plugin + '")', model)

    def test_vfx_content_pack_review_map_is_explicit_editor_allowlist(self):
        """真实寒冰箭审核地图属于内容包而不是/Game/Development；仅白名单只读诊断。"""
        catalog, model = read(CORE), read(MODEL)
        package = "/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review/L_DBA_IceArrow_A_Review"
        review = (ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
                  "DBAPresentationPack_Core/Content/VFX/IceArrow/Maps/Review/"
                  "L_DBA_IceArrow_A_Review.umap")
        self.assertTrue(review.is_file())
        with review.open("rb") as source:
            self.assertEqual(MAGIC, source.read(4))
        self.assertIn('Registry.GetAssetsByPath(FName(TEXT("/DBAPresentationPack_Core/VFX/IceArrow/Maps/Review"))', catalog)
        self.assertIn('Package == TEXT("' + package + '")', catalog)
        self.assertIn('Package == TEXT("' + package + '")', model)
        self.assertIn('PluginId == TEXT("GamePlatformVFX")', model)
        self.assertIn('PluginId == TEXT("DBAPresentationPack_Core")', model)
        self.assertIn('const bool bApprovedVFXReview = false', catalog)
        self.assertIn('#if WITH_EDITOR', catalog)
        self.assertIn('Item.bDiagnosticOnly = true', catalog)
        self.assertIn('CurrentMap', read(WORLD))

    def test_disabled_domain_cannot_offer_diagnostic_launch(self):
        model = read(MODEL)
        self.assertIn('SelectedDiagnosticPlugin->bEnabled', model)
        self.assertIn('Snapshot.SelectedFeatureId.IsNone()', model)
        self.assertIn('SelectedDiagnosticPlugin->PluginId == TEXT("GamePlatformDeveloperTools")', model)

    def test_search_list_entries_are_reused_once_per_stable_identity(self):
        """菜单搜索不能逐键重新创建全部值对象，也不能把同一对象放入多个列表。"""
        screen = read(SCREEN)
        self.assertIn("TMap<FName, UDivineBeastsDemoMenuItem*> ReusableById", screen)
        self.assertIn("ReusableById.Find(Identity)", screen)
        self.assertIn("ReusableById.Remove(Identity)", screen)
        self.assertIn("if (bReusedAny) List->RequestRefresh()", screen)
        self.assertIn("if (!Item) Item = NewObject<UDivineBeastsDemoMenuItem>(this)", screen)

    def test_project_vfx_review_map_not_selected_for_dedicated_server(self):
        """确认项目唯一Target装配不会将开发VFX内容包加入专用服务器。"""
        import json
        project = json.loads((ROOT / "Game/DivineBeastsArena.uproject").read_text(encoding="utf-8-sig"))
        selected = [p for p in project["Plugins"] if p.get("Name") == "DBAPresentationPack_Core"]
        self.assertEqual(1, len(selected))
        self.assertTrue(selected[0]["Enabled"])
        self.assertNotIn("Server", selected[0].get("TargetAllowList", []))
        self.assertIn("Editor", selected[0].get("TargetAllowList", []))

    def test_plugin_search_ignores_ascii_case_without_executing_filter(self):
        model = read(MODEL)
        self.assertGreaterEqual(model.count("Contains(Query, ESearchCase::IgnoreCase)"), 6)
        self.assertIn("FeatureMatchesText", model)

    def test_buttons_show_unapproved_diagnostic_and_return(self):
        screen = read(SCREEN)
        overlay = read(OVERLAY)
        world = read(WORLD)
        self.assertIn('S.Scenario.TemplateId == TEXT("DiagnosticMap")', screen)
        self.assertIn("仅供人工诊断，不计功能验收", screen)
        self.assertIn("LaunchScenarioButton->SetIsEnabled", screen)
        self.assertIn("OnLaunchClicked()", screen)
        self.assertIn("OnHubReturnClicked()", overlay)
        self.assertIn("RequestReturnToHub()", overlay)
        self.assertIn("OnWorldBeginPlay(", world)
        self.assertIn("RequestDemoOverlay(", world)
        self.assertIn("Widget->AddToViewport(100)", world)


if __name__ == "__main__":
    unittest.main(verbosity=2)
