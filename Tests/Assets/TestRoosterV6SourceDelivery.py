#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""酉鸡V6源图交付回归：真实原包、稳定身份和损坏输入；不加载UE或创建引擎资产。

只在临时目录复制原包和构造故障输入，验证既有导入器的纯文件预检。
本测试通过不代表Texture2D、界面配置或运行时预览已验收。
"""
import contextlib
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
import zipfile
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
PLAN_PATH = Path('Docs/Implementation/GamePlatformAbilitySystem/RoosterV6SkillIconImportManifest_20261019.json')
ARCHIVE_PATH = Path('Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rooster/SourceArt/UI/SkillIcons/V6/Delivery/SSLM_RoosterV6_8SkillIcons_ReadyForUE58.zip')


class RoosterSourceDeliveryTests(unittest.TestCase):
    """生产回归目标：丢失仓库回退、错误回退掩盖坏包或身份漂移均须失败。"""

    @classmethod
    def setUpClass(cls):
        cls.plan = json.loads((ROOT / PLAN_PATH).read_text(encoding='utf-8'))
        cls.archive = ROOT / ARCHIVE_PATH
        spec = importlib.util.spec_from_file_location(
            'rooster_source_preflight', ROOT / 'Tools/Unreal/UI/ImportRoosterV6SkillIconsUE58.py')
        cls.importer = importlib.util.module_from_spec(spec)
        with patch.dict(os.environ, {'SSLM_ROOSTER_V6_MODE': 'inspect'}), contextlib.redirect_stdout(io.StringIO()):
            spec.loader.exec_module(cls.importer)

    def setUp(self):
        # 故障图包只属于本次临时夹具，不写入英雄包或真实编辑器。
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.plan_copy = json.loads(json.dumps(self.plan))
        self.plan_copy['repositoryZipRelativePath'] = ARCHIVE_PATH.as_posix()
        self.plan_file = self.root / PLAN_PATH
        self.plan_file.parent.mkdir(parents=True)
        self.repo_archive = self.root / ARCHIVE_PATH
        self.repo_archive.parent.mkdir(parents=True)
        shutil.copyfile(self.archive, self.repo_archive)
        self.save_plan()
        self.addCleanup(patch.stopall)
        patch.object(self.importer, 'WORKSPACE', self.root).start()
        patch.object(self.importer, 'PLAN', self.plan_file).start()
        patch.object(self.importer, 'MODE', 'inspect').start()

    def save_plan(self):
        """写入临时清单，允许逐个注入可解释的故障。"""
        self.plan_file.write_text(json.dumps(self.plan_copy), encoding='utf-8')

    def preflight(self):
        """执行生产预检并抑制常规进度文本，保留原始异常。"""
        with contextlib.redirect_stdout(io.StringIO()):
            return self.importer.load_inputs()

    def test_original_archive_matches_frozen_sha(self):
        self.assertEqual(hashlib.sha256(self.archive.read_bytes()).hexdigest(), self.plan['zipSha256'])
        with zipfile.ZipFile(self.archive) as archive:
            self.assertIsNone(archive.testzip())

    def test_internal_manifest_and_all_image_hashes_match(self):
        """交付包内外身份一致；母稿、64审核和48运行图均按原包清单逐项校验。"""
        with zipfile.ZipFile(self.archive) as archive:
            inside = json.loads(archive.read('Docs/RoosterV6SkillIconManifest.json'))
            self.assertEqual(inside['heroId'], self.plan['heroId'])
            self.assertEqual(len(inside['iconEntries']), 8)
            for outer, inner in zip(self.plan['entries'], inside['iconEntries']):
                for field in ('order', 'slotId', 'abilityId'):
                    self.assertEqual(outer[field], inner[field])
                self.assertEqual(outer['displayNameZh'], inner['displayNameV6'])
                self.assertEqual(outer['texturePath'], inner['uiTexturePackage'])
                for outer_key, inner_key, hash_key in (
                        ('master', 'sourceFile', 'sourceSha256'),
                        ('review64', 'preview64', 'review64Sha256'),
                        ('runtime48', 'runtime48', 'runtime48Sha256')):
                    self.assertEqual(outer[outer_key], inner[inner_key])
                    self.assertEqual(hashlib.sha256(archive.read(outer[outer_key])).hexdigest(), inner[hash_key])

    def test_repository_archive_supplies_all_24_images_without_staging(self):
        _, archive, images = self.preflight()
        self.assertEqual(archive, self.repo_archive)
        self.assertEqual(len(images), 24)

    def test_legacy_staged_archive_is_still_accepted(self):
        staged = self.root / self.plan_copy['zipRelativePath']
        staged.parent.mkdir(parents=True)
        shutil.copyfile(self.archive, staged)
        self.assertEqual(self.preflight()[1], staged)

    def test_bad_staged_archive_is_not_hidden_by_repository_fallback(self):
        staged = self.root / self.plan_copy['zipRelativePath']
        staged.parent.mkdir(parents=True)
        staged.write_bytes(b'corrupt test fixture')
        with self.assertRaisesRegex(RuntimeError, 'ZIP.*SHA-256'):
            self.preflight()

    def test_missing_archive_cannot_enter_engine_import(self):
        self.repo_archive.unlink()
        self.assertIsNone(self.preflight()[1])
        with patch.object(self.importer, 'MODE', 'import'), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(RuntimeError, '缺图片压缩包'):
                self.importer.main()

    def test_changed_runtime_image_is_rejected_even_with_updated_zip_hash(self):
        member = self.plan_copy['entries'][0]['runtime48']
        original = self.repo_archive.read_bytes()
        with zipfile.ZipFile(io.BytesIO(original)) as source, zipfile.ZipFile(self.repo_archive, 'w') as target:
            for item in source.infolist():
                blob = source.read(item.filename)
                target.writestr(item, blob + b'fault' if item.filename == member else blob)
        self.plan_copy['zipSha256'] = hashlib.sha256(self.repo_archive.read_bytes()).hexdigest()
        self.save_plan()
        with self.assertRaisesRegex(RuntimeError, '正式48纹理SHA不匹配'):
            self.preflight()

    def test_archive_path_cannot_leave_workspace(self):
        self.plan_copy['repositoryZipRelativePath'] = '../outside.zip'
        self.save_plan()
        with self.assertRaisesRegex(RuntimeError, '工作空间'):
            self.preflight()

    def test_registry_identity_order_and_server_grants_remain_frozen(self):
        registry = json.loads((ROOT / 'Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json').read_text(encoding='utf-8'))
        rows = sorted((r for r in registry['skills'] if r['heroId'] == 'Hero.Zodiac.Rooster'), key=lambda r: r['displayOrder'])
        self.assertEqual(len(rows), 8)
        for manifest, skill in zip(self.plan['entries'], rows):
            self.assertEqual((manifest['abilityId'], manifest['slotId'], manifest['displayNameZh']),
                             (skill['abilityId'], skill['slotId'], skill['displayNameV6']))
            self.assertFalse(skill['canGrantToServer'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
