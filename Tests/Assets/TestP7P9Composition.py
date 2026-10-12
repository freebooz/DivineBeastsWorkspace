#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三批源资产归属与端侧装配合同；不代替真实Cook与人工试听。"""
from pathlib import Path
import json
import unittest
ROOT=Path(__file__).resolve().parents[2]
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core'
class P7P9CompositionTest(unittest.TestCase):
    def test_audio_pack_registered_once(self):
        entries=json.loads((ROOT/'Game/Plugins/DivineBeasts/ContentPacks/ContentPackRegistry.json').read_text(encoding='utf-8-sig'))['ContentPacks']
        self.assertEqual(sum(x['Name']=='DBASFXPack_Core' for x in entries),1)
    def test_audio_pack_contains_no_code_modules(self):
        desc=PACK/'DBASFXPack_Core.uplugin'
        self.assertTrue(desc.is_file(),'独立音效内容包描述未建立')
        data=json.loads(desc.read_text(encoding='utf-8-sig'))
        self.assertTrue(data['CanContainContent']);self.assertFalse(data.get('Modules',[]))
        self.assertFalse(data['EnabledByDefault'])
    def test_audio_pack_dependencies_restricted(self):
        desc=PACK/'DBASFXPack_Core.uplugin'
        self.assertTrue(desc.is_file())
        for dep in json.loads(desc.read_text(encoding='utf-8-sig')).get('Plugins',[]):
            self.assertEqual(set(dep['TargetAllowList']),{'Client','Editor'})
    def test_client_editor_audio_enabled(self):
        for role in ('Client','Editor'):
            source=(ROOT/f'Game/Source/DivineBeastsArena{role}.Target.cs').read_text(encoding='utf-8-sig')
            self.assertIn('EnablePlugins.Add("DBASFXPack_Core")',source)
    def test_server_audio_explicitly_disabled(self):
        source=(ROOT/'Game/Source/DivineBeastsArenaServer.Target.cs').read_text(encoding='utf-8-sig')
        self.assertIn('DisablePlugins.Add("DBASFXPack_Core")',source)
        self.assertNotIn('EnablePlugins.Add("DBASFXPack_Core")',source)
    def test_client_editor_player_service_composition(self):
        for role in ('Client','Editor'):
            source=(ROOT/f'Game/Source/DivineBeastsArena{role}.Target.cs').read_text(encoding='utf-8-sig')
            for name in ('GamePlatformCommerceUI','GamePlatformEntitlement','GamePlatformEquipment','GamePlatformLiveOps','GamePlatformProgression'):
                self.assertIn(f'EnablePlugins.Add("{name}")',source)
    def test_server_does_not_enable_pure_client_services(self):
        source=(ROOT/'Game/Source/DivineBeastsArenaServer.Target.cs').read_text(encoding='utf-8-sig')
        for name in ('GamePlatformCommerceUI','GamePlatformLiveOps','GamePlatformSFX'):
            self.assertNotIn(f'EnablePlugins.Add("{name}")',source)
    def test_development_maps_keep_original_release_exclusion(self):
        source=(ROOT/'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
        self.assertIn('DirectoriesToNeverCook=(Path="/Game/Development/Foundation")',source)
        self.assertIn('/DBASFXPack_Core/Weather/Definitions',source)
    def test_client_config_cooks_sound_definitions_without_full_pack(self):
        # 真实Cook曾遗漏按逻辑身份加载的声音；只显式收录正式Definitions，不强制整个声音目录。
        for config in ('FrontEndClient','VillageDevelopmentClient'):
            source=(ROOT/f'Game/Config/Custom/{config}/DefaultGame.ini').read_text(encoding='utf-8-sig')
            self.assertIn('+DirectoriesToAlwaysCook=(Path="/DBASFXPack_Core/Weather/Definitions")',source)
            self.assertNotIn('+DirectoriesToAlwaysCook=(Path="/DBASFXPack_Core")',source)
    def test_formal_publish_keeps_new_audio_container_gate(self):
        source=(ROOT/'Build/Game/CookFrontEndClient.ps1').read_text(encoding='utf-8-sig')
        self.assertIn('Build/Validation/PluginDeliveryEvidence.py',source)
        self.assertIn('--container-csv',source)
        self.assertIn('P7ContentGate',source)
        self.assertIn('TestUIColorRHI.ps1',source)
        self.assertIn('ValidateBirdFlockContainer.py',source)
if __name__=='__main__':unittest.main(verbosity=2)
