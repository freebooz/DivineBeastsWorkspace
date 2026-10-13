#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""开发施法交付的配置回归：目录语义、唯一Data扫描、显式怪物运行开关。
只验证人工维护接线，不代替UE资源重载、运行时表现、联机伤害或Cook验收。
"""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class DevelopmentCastingConfiguration(unittest.TestCase):
    def test_catalog_uses_registered_project_semantic(self):
        """目录ID不是GameplayTag；九条目必须采用实际注册的DivineBeasts语义前缀。"""
        source = (ROOT / 'Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Adapters/DivineBeastsDevelopmentMagicPresentation.cpp').read_text(encoding='utf-8-sig')
        self.assertNotIn('TEXT("Presentation.")+Entry.EntryId.ToString()', source)
        self.assertIn('TEXT("Presentation.DivineBeasts.Development.Magic.")', source)
        self.assertIn('TEXT("Cast")', source)

    def test_cast_definitions_are_scanned_by_existing_asset_manager(self):
        """起手定义不能因位于Definitions的兄弟目录而被遗漏；仍使用原唯一主资产类型。"""
        config = (ROOT / 'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
        scans = [line for line in config.splitlines() if line.startswith('+PrimaryAssetTypesToScan=')]
        self.assertTrue(any('PrimaryAssetType="GamePlatformDefinition"' in line and
                            '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Definitions' in line for line in scans))

    def test_visible_launcher_explicitly_enables_development_monsters(self):
        """客户端必须显式选择开发怪物；此检查不启动窗口或认证，也不修改用户环境。"""
        script = (ROOT / 'Build/Game/RunFrontEndClients.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('[switch]$DevelopmentMonsters', script)
        self.assertIn("$info.ArgumentList.Add('-DBADevelopmentMonsters')", script)
        self.assertIn('DevelopmentMonsters=[bool]$DevelopmentMonsters', script)
        self.assertIn('$DevelopmentMonsters -and !$DevelopmentSkills', script)


if __name__ == '__main__':
    unittest.main()
