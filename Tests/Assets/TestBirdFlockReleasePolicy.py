#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群发布门禁回归；合成CSV仅测试策略，不伪装实际Cook或GPU运行。
导入真实校验器，验证19项资源、历史模型/附属块/开发图禁入和输入错误；
同时核对实际两个客户端配置。只写临时目录，不改项目资源或历史证据。
"""
from pathlib import Path
import csv
import importlib.util
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('bird_release', ROOT/'Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockContainer.py')
assert SPEC and SPEC.loader
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
DBA = '../../../DivineBeastsArena/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock'
GP = '../../../DivineBeastsArena/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock'


def required_files():
    """独立列出规格19项，避免实现常量出错时测试同步出错。"""
    names = [GP+'/Materials/M_GP_BirdFlock_Master.uasset', GP+'/Niagara/NS_GP_BirdFlock_Base.uasset',
             GP+'/EffectTypes/NET_GP_BirdFlock_Ambient.uasset', DBA+'/Blueprints/BP_DBA_BirdFlockAmbient.uasset']
    for species in ('Seagull', 'Swallow', 'Eagle'):
        names.extend([DBA+'/Meshes/Flight/SM_DBA_Bird_'+species+'.uasset',
                      DBA+'/Textures/T_DBA_Bird_'+species+'_Albedo.uasset',
                      DBA+'/Materials/MI_DBA_Bird_'+species+'.uasset',
                      DBA+'/Niagara/NS_DBA_BirdFlock_'+species+'.uasset',
                      DBA+'/Definitions/DA_DBA_VFX_BirdFlock_'+species+'.uasset'])
    return names


class BirdFlockReleasePolicyTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='bird-release-')
        self.addCleanup(self.tmp.cleanup)
        self.csv = Path(self.tmp.name)/'Container.csv'

    def report(self, files, target='Client', platform='WindowsClient'):
        with self.csv.open('w', encoding='utf-8-sig', newline='') as stream:
            writer = csv.writer(stream)
            writer.writerow(('Filename', 'Platform'))
            writer.writerows((name, platform) for name in files)
        return VALIDATOR.validate(self.csv, target)

    def test_clean_client_requires_all_19_assets(self):
        result = self.report(required_files())
        self.assertEqual(result['status'], 'passed')
        self.assertEqual(result['requiredCount'], 19)

    def test_missing_species_asset_is_rejected(self):
        self.assertEqual(self.report(required_files()[:-1])['status'], 'failed')

    def test_legacy_mesh_main_and_sidecar_blocks_are_rejected(self):
        for suffix in ('.uasset', '.ubulk', '.uexp', '_Cm.uasset', '_Flight.uasset'):
            with self.subTest(suffix=suffix):
                result = self.report(required_files()+[DBA+'/Meshes/SM_DBA_Bird_Seagull'+suffix])
                self.assertEqual(result['status'], 'failed')
                self.assertTrue(result['forbidden'])

    def test_legacy_mixed_case_windows_path_is_rejected(self):
        old = (DBA+'/Meshes/SM_DBA_Bird_Eagle_Cm.uasset').upper().replace('/', '\\')
        self.assertEqual(self.report(required_files()+[old])['status'], 'failed')

    def test_case_insensitive_formal_paths_are_recognized(self):
        names = [p.upper().replace('/', '\\') for p in required_files()]
        self.assertEqual(self.report(names)['status'], 'passed')

    def test_source_art_is_rejected(self):
        self.assertEqual(self.report(required_files()+[DBA+'/SourceArt/model.blend'])['status'], 'failed')

    def test_development_review_map_is_rejected(self):
        path = '../../../DivineBeastsArena/Content/Development/BirdFlock/Maps/L_DBA_BirdFlockReview.umap'
        self.assertEqual(self.report(required_files()+[path])['status'], 'failed')

    def test_external_actor_review_packages_are_rejected(self):
        # 世界分区外置Actor/对象不直接位于Content/Development，仍属于禁止发布的审核图。
        for folder in ('__ExternalActors__', '__ExternalObjects__'):
            path = '../../../DivineBeastsArena/Content/'+folder+'/Development/BirdFlock/Maps/A/BC/Actor.uasset'
            with self.subTest(folder=folder):
                self.assertEqual(self.report(required_files()+[path])['status'], 'failed')

    def test_server_must_have_no_bird_assets(self):
        self.assertEqual(self.report([required_files()[0]], 'Server', 'WindowsServer')['status'], 'failed')

    def test_clean_server_accepts_unrelated_runtime_files(self):
        path = '../../../DivineBeastsArena/Content/World/ServerMap.umap'
        self.assertEqual(self.report([path], 'Server', 'WindowsServer')['status'], 'passed')

    def test_server_rejects_client_platform_evidence(self):
        with self.assertRaises(ValueError):
            self.report(['../../../DivineBeastsArena/Content/World/Map.umap'], 'Server', 'WindowsClient')

    def test_unsupported_target_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report(required_files(), 'Unknown')

    def test_empty_csv_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report([])

    def test_missing_platform_column_is_rejected(self):
        self.csv.write_text('Filename\n'+required_files()[0]+'\n', encoding='utf-8')
        with self.assertRaises(ValueError):
            VALIDATOR.validate(self.csv, 'Server')

    def test_blank_platform_is_rejected(self):
        with self.assertRaises(ValueError):
            self.report(required_files(), platform='')

    def test_client_configs_include_only_formal_bird_subdirectories(self):
        mount = '/DBAPresentationPack_Core/VFX/Environment/BirdFlock'
        for config in ('FrontEndClient', 'VillageDevelopmentClient'):
            with self.subTest(config=config):
                text = (ROOT/'Game/Config/Custom'/config/'DefaultGame.ini').read_text(encoding='utf-8-sig')
                self.assertNotIn('+DirectoriesToAlwaysCook=(Path="'+mount+'")', text)
                for child in ('Definitions', 'Niagara', 'Materials', 'Textures', 'Meshes/Flight', 'Blueprints'):
                    self.assertIn('+DirectoriesToAlwaysCook=(Path="'+mount+'/'+child+'")', text)

    def test_default_release_excludes_review_map(self):
        text = (ROOT/'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
        self.assertIn('+DirectoriesToNeverCook=(Path="/Game/Development/BirdFlock")', text)

    def test_cook_entry_enforces_bird_gate_and_existing_ui_gate(self):
        text = (ROOT/'Build/Game/CookFrontEndClient.ps1').read_text(encoding='utf-8-sig')
        for gate in ('ValidateBirdFlockContainer.py', 'TestUIColorRHI.ps1', 'TestFrontEndClientContainer.ps1'):
            self.assertIn(gate, text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
