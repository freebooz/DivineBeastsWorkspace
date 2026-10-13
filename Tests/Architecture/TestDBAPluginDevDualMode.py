#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟双开发模式的静态入口/端侧/配置防回归测试。

本测试不启动UE；资源存在性与源码合同不代表原生C++已编译、UE世界进退或人工验收通过。
验证目标：Debug沿用登录，PluginDev需要双重显式开发标志，不能污染发行或专服。
"""
from __future__ import annotations
import json
import re
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub"


def read(name: str) -> str:
    return (ROOT/name).read_text(encoding="utf-8-sig")


class DualModePolicyTests(unittest.TestCase):
    def test_debug_is_default_mode(self):
        source=read("Build/Game/LaunchDBADevelopment.ps1")
        self.assertIn("[string]$Mode='Debug'",source)
        self.assertIn("'/DBAFrontEndPack/Maps/L_DBA_FrontEnd'",source)
        self.assertIn("ManualAuthentication=$true",source)
        self.assertIn("DemoBypassEnabled=$false",source)

    def test_plugin_mode_delegates_existing_single_launcher(self):
        source=read("Build/Game/LaunchDBADevelopment.ps1")
        self.assertIn("'PluginDev'",source)
        self.assertIn("'RunPluginDemo.ps1'",source)
        self.assertIn("if($Mode -eq 'PluginDev')",source)

    def test_real_demo_requires_explicit_optin_and_correct_map(self):
        script=read("Build/Game/RunPluginDemo.ps1")
        policy=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Private/Development/DivineBeastsPluginDemoMode.cpp")
        self.assertIn(MAP,script)
        self.assertIn("'-DBAPluginDemo'",script)
        self.assertIn('-DBAPluginDemoMap=$map',script)
        self.assertIn(MAP,policy)
        self.assertIn("!bConflictingDevelopmentMode",policy)
        self.assertIn("#if UE_BUILD_SHIPPING || UE_BUILD_TEST",policy)

    def test_plugin_launcher_requires_real_hub_widget(self):
        """演示启动必须具备实际Monolith界面资产，不能只打开空白关卡。"""
        launcher=read("Build/Game/RunPluginDemo.ps1")
        self.assertIn("WBP_DBA_PluginDemoHub.uasset",launcher)
        self.assertIn("Test-Path -LiteralPath $hubWidgetFile -PathType Leaf",launcher)

    def test_editor_client_server_are_isolated_by_module_type(self):
        plugin=json.loads(read("Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin"))
        modules=[x for x in plugin["Modules"] if x["Name"]=="DivineBeastsDemoClient"]
        self.assertEqual(len(modules),1)
        self.assertEqual(modules[0]["Type"],"ClientOnly")
        self.assertEqual(set(modules[0]["TargetConfigurationAllowList"]),
            {"Debug","DebugGame","Development"})

    def test_formal_default_still_login_and_review_never_cooked(self):
        engine=read("Game/Config/DefaultEngine.ini")
        packaging=read("Game/Config/DefaultGame.ini")
        self.assertRegex(engine,r"(?m)^GameDefaultMap=/DBAFrontEndPack/Maps/L_DBA_FrontEnd$")
        self.assertIn('DirectoriesToNeverCook=(Path="/Game/Development/PluginDemos")',packaging)

    def test_real_application_flow_bypass_prevents_start(self):
        adapter=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Adapters/Application/DivineBeastsApplicationUIAdapter.cpp")
        flow=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Private/DivineBeastsApplicationFlowSubsystem.cpp")
        self.assertIn("FDivineBeastsPluginDemoMode::IsEnabled()",adapter)
        self.assertIn("StartFlow(false)",adapter)
        self.assertIn("FDivineBeastsPluginDemoMode::IsEnabled()",flow)

    def test_development_ui_does_not_start_login_widget(self):
        ui=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/DivineBeastsUIClientSubsystem.cpp")
        self.assertGreaterEqual(ui.count("FDivineBeastsPluginDemoMode::IsEnabled()"),2)
        self.assertIn("Super::PlayerControllerChanged(NewPlayerController)",ui)

    def test_portal_data_and_navigation_api_exists(self):
        header=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Public/Demo/DivineBeastsDemoCatalogSubsystem.h")
        for api in ("RefreshCatalog","GetPlugins","GetScenarios","OpenScenario","ReturnToHub"):
            with self.subTest(api=api):
                self.assertIn(api+"(",header)

    def test_portal_filter_and_input_lifecycle_code_is_present(self):
        """静态回归：蓝图可筛选发现列表；真实Widget到位后应能获得鼠标焦点并释放输入。"""
        widget_header=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Public/UI/DivineBeastsDemoHubWidget.h")
        widget_cpp=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/UI/DivineBeastsDemoHubWidget.cpp")
        world_cpp=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/Demo/DivineBeastsDemoWorldSubsystem.cpp")
        self.assertIn("FilterDiscoveredPlugins(",widget_header)
        for contract in ("Plugin.Layer != Layer", "Plugin.DiscoveredMapCount == 0", "ESearchCase::IgnoreCase"):
            self.assertIn(contract,widget_cpp)
        for contract in ("FInputModeGameAndUI", "SetWidgetToFocus", "SetInputMode(InputMode)",
                         "bShowMouseCursor = true", "DemoInputController.Reset()"):
            self.assertIn(contract,world_cpp)

    def test_real_plugin_demo_assets_exist(self):
        """仅验证UE/Monolith产物在工程中落盘；不将存在性冒充运行或人工验收。"""
        for asset in (
            "Game/Content/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.umap",
            "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoHub.uasset",
            "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoOverlay.uasset",
            "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoBirdFlock.uasset",
        ):
            with self.subTest(asset=asset):
                path=ROOT/asset
                self.assertTrue(path.is_file())
                self.assertGreater(path.stat().st_size,1024)

    def test_named_widgets_have_native_interaction_bridge(self):
        """搜索、分类、场景导航与返回桥接到真实项目目录，拒绝在C++伪造可视控件树。"""
        header=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Public/UI/DivineBeastsDemoHubWidget.h")
        source=read("Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/UI/DivineBeastsDemoHubWidget.cpp")
        for widget in ("PluginSearchInput", "LayerFilterCombo", "PluginSelectorCombo",
                       "ScenarioSelectorCombo", "RefreshButton", "LaunchScenarioButton",
                       "ReturnButton", "OverlayFeedbackText"):
            with self.subTest(widget=widget):
                self.assertIn(widget,header)
        for handler in ("OnSearchChanged", "OnLayerChanged", "OnPluginChanged",
                        "OnScenarioChanged", "OnScenarioLaunchClicked",
                        "OnHubReturnClicked", "OnDemoResetClicked"):
            with self.subTest(handler=handler):
                self.assertIn(handler+"(",source)
        self.assertIn("GetWidgetFromName(TEXT(\"BackButton\"))",source)
        self.assertNotIn("TObjectPtr<class UButton> BackButton;",header)

    def test_development_entry_has_no_password_parameter(self):
        source=read("Build/Game/LaunchDBADevelopment.ps1")
        self.assertNotRegex(source,r"(?i)-DBADevAutoLogin")
        self.assertNotRegex(source,r"(?i)-DBADevLoginUser")
        self.assertIn("DBA_DEV_LOGIN_SECRET",source)


if __name__=="__main__":
    unittest.main(verbosity=2)
