# -*- coding: utf-8 -*-
"""SSLM 插件化 UI 整改回归：公共/竞技目录、实体UI职责和预览安全期限。

所有变异仅修改内存对象；不启动UE、修改WBP、生成假UI或改变其他会话数据。
实际C++运行/蓝图父类反射和时钟调度另由UE5.8原生自动化验收。
"""
from __future__ import annotations

import copy
import unittest
from pathlib import Path

from ValidateUICatalogNativeAssets import (
    ROOT, PUBLIC_CATALOG, ARENA_CATALOG, catalog_entries, audit, required_bind_names
)

UI_SOURCE = ROOT / ("Game/Plugins/DivineBeasts/DBAClient/Source/"
                    "DivineBeastsUIClient/Private/DivineBeastsUIClientSubsystem.cpp")
WORLD_APPEARANCE = ROOT / ("Game/Plugins/DivineBeasts/DBAClient/Source/"
                          "DivineBeastsPresentationClient/Private/Characters/"
                          "DivineBeastsCharacterAppearanceWorldSubsystem.cpp")
SETTINGS_VM = ROOT / ("Game/Plugins/DivineBeasts/DBAClient/Source/"
                     "DivineBeastsUIClient/Private/ViewModels/System/"
                     "DivineBeastsSystemSettingsViewModel.cpp")


def current_catalog():
    """使用当前真实源码，而非内存里人工写出的页面和资源名。"""
    return catalog_entries(PUBLIC_CATALOG.read_text(encoding="utf-8-sig"),
                           ARENA_CATALOG.read_text(encoding="utf-8-sig"))


class UIArchitectureRemediationTests(unittest.TestCase):
    """损坏路径、重复ID和错误父类必须被静态门禁拒绝。"""

    def test_real_asset_catalog_passes(self):
        result = audit(current_catalog())
        self.assertEqual([], result["issues"], result["issues"])
        self.assertEqual(18, result["publicCatalog"])
        self.assertEqual(6, result["arenaCatalog"])
        self.assertEqual(result["registeredUI"], result["nativeAssetParentNamePresent"])
        self.assertEqual(result["requiredBindWidgetCount"], result["requiredNamesFound"])

    def test_duplicate_screen_identity_rejected(self):
        catalog = current_catalog()
        catalog.append(copy.deepcopy(catalog[0]))
        self.assertTrue(any("重复ScreenId" in item for item in audit(catalog)["issues"]))

    def test_missing_wbp_rejected(self):
        catalog = current_catalog()
        catalog[0]["asset"] = str(ROOT / "Game/Nonexistent/UI/WBP_Invalid.uasset")
        self.assertTrue(any("无真实WBP" in item for item in audit(catalog)["issues"]))

    def test_wrong_parent_rejected(self):
        catalog = current_catalog()
        catalog[0]["parent"] = "SSLM_IllegalFakeParent"
        self.assertTrue(any("原生父类符号" in item for item in audit(catalog)["issues"]))

    def test_required_vs_optional_bindings(self):
        source = ("UPROPERTY(meta=(BindWidget))\n"
                  "TObjectPtr<UButton> SubmitButton = nullptr;\n"
                  "UPROPERTY(meta=(BindWidgetOptional))\n"
                  "TObjectPtr<UButton> DecorativeButton = nullptr;")
        self.assertEqual(["SubmitButton"], required_bind_names(source))

    def test_world_appearance_no_longer_owned_by_ui(self):
        ui = UI_SOURCE.read_text(encoding="utf-8-sig")
        world = WORLD_APPEARANCE.read_text(encoding="utf-8-sig")
        self.assertNotIn("ApplyApprovedVisualHero(", ui)
        self.assertNotIn("NewObject<UDivineBeastsCharacterAppearanceComponent>", ui)
        self.assertIn("Preview->DeactivatePreviewScene()", ui)
        self.assertIn("World->AddOnActorSpawnedHandler", world)
        self.assertIn("OnWorldBeginPlay(", world)
        self.assertIn("GetNetMode() == NM_DedicatedServer", world)

    def test_settings_timeout_is_deadline_event_with_retry(self):
        source = SETTINGS_VM.read_text(encoding="utf-8-sig")
        self.assertIn("SchedulePreviewDeadline(GetPageGeneration(), 15.0)", source)
        self.assertIn("PreviewDeadlineSeconds - FPlatformTime::Seconds()", source)
        self.assertIn("SchedulePreviewDeadline(ExpectedPageGeneration, 1.0)", source)
        self.assertIn("GetPageGeneration() != ExpectedPageGeneration", source)
        self.assertNotIn('TEXT("DBA.SettingsPreviewTimeout"), 0.1f', source)
        self.assertIn("return false; // 一次性到期事件", source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
