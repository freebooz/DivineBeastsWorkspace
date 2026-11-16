#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM简洁优选名称批准后的数据一致性回归。

读取用户确认的96行名称快照及现有V2/V3数据；不启动UE、不编译、不读写.uasset。
名称通过不等于机制、数值、图片或游戏运行通过，相关准入必须继续失败关闭。
"""
from __future__ import annotations
import json
import hashlib
import copy
import importlib.util
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REGISTRY = ROOT / 'Docs/Design/ApprovedSkillDisplayNames_V5.json'
V3 = ROOT / 'Docs/Design/ZodiacSkillV3Review'
APPROVAL = 'SSLM-SKILL-NAMES-SIMPLE-V5-20261018'


def load(path):
    return Path(path).read_text(encoding='utf-8-sig')


def data(path):
    return json.loads(load(path))


class ApprovedSkillDisplayNamesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.registry = data(REGISTRY)
        cls.entries = [dict(zip(cls.registry['entryColumns'], row)) for row in cls.registry['entries']]
        cls.names = {row['abilityId']: row for row in cls.entries}
        cls.v2 = data(ROOT / 'Docs/Design/ZodiacEightSlotSkills_V2.json')
        cls.index = data(V3 / 'ReviewIndex.json')
        cls.heroes = {h['heroId']: data(V3 / Path(h['reviewFile']).name) for h in cls.index['heroRecords']}

    def test_01_source_is_latest_simple_workbook_and_name_only_approval(self):
        self.assertEqual(self.registry['sourceWorkbookSha256'], '1e86f4d6bfb17b9bb4262f3f6941bd16c13c2b53f15933ea5254ee41248f62c1')
        self.assertEqual(self.registry['sourceRange'], 'G7:G102')
        self.assertEqual(self.registry['approvalScope'], 'DISPLAY_NAMES_ONLY')
        self.assertEqual(len(self.entries), 96)
        self.assertEqual(len(self.names), 96)
        # 该指纹由本对话原Excel实际单元格读取生成，防止转录时姓名/技能编号串行。
        fingerprint = hashlib.sha256(json.dumps(self.registry['entries'], ensure_ascii=False,
                                    separators=(',', ':')).encode('utf-8')).hexdigest()
        self.assertEqual(fingerprint, '74606f705f417b0bafb56f39c0fe4c825b23ef9165a8e7b2af584d8fde52840d')
        self.assertEqual(len({r['approvedNameZh'] for r in self.entries}), 96)
        self.assertFalse(self.registry['approveEffectImages'])
        self.assertFalse(self.registry['approveGameplay'])
        for index, row in enumerate(self.entries, 7):
            self.assertEqual(row['sourceCell'], f'G{index}')
            self.assertRegex(row['approvedNameZh'], r'^[\u4e00-\u9fff]{2,3}$')
        self.assertEqual(sum(r['approvedNameZh'] != r['previousV3NameZh'] for r in self.entries), 67)

    def test_02_all_v3_display_names_approved_and_historical_alias_retained(self):
        seen = set()
        for hero in self.heroes.values():
            for row in hero['skillRows']:
                with self.subTest(id=row['abilityId']):
                    approved = self.names[row['abilityId']]
                    self.assertEqual(row['nameZh'], approved['approvedNameZh'])
                    self.assertEqual(row['status']['skillName'], 'APPROVED_USER_SIMPLE_V5')
                    self.assertEqual(row['previousNameBeforeSimpleV5'], approved['previousV3NameZh'])
                    self.assertEqual(row['nameApprovalId'], APPROVAL)
                    seen.add(row['abilityId'])
        self.assertEqual(seen, set(self.names))

    def test_03_data_asset_author_source_has_approved_names_without_enabling_gameplay(self):
        seen = set()
        for hero in self.v2['heroes']:
            for row in hero['slots']:
                with self.subTest(id=row['abilityId']):
                    self.assertEqual(row['nameZh'], self.names[row['abilityId']]['approvedNameZh'])
                    self.assertFalse(row['approvedForLiveGAS'])
                    self.assertEqual(row['implementationStatus'], 'DESIGN_ONLY_UNIMPLEMENTED')
                    seen.add(row['abilityId'])
        self.assertEqual(seen, set(self.names))

    def test_04_creation_text_and_image_prompt_titles_match_current_name(self):
        for hero in self.heroes.values():
            for row in hero['skillRows']:
                quoted = '「' + row['nameZh'] + '」'
                with self.subTest(id=row['abilityId']):
                    self.assertIn(quoted, row['creatorDescriptionZh'])
                    self.assertIn(quoted, row['imageConcept']['shortPromptZh'])
                    self.assertFalse(row['status']['canShowInProductionCreation'])
                    self.assertEqual(row['status']['mechanics'], 'PENDING_HUMAN_REVIEW')
                    self.assertEqual(row['status']['effectImage'], 'NOT_GENERATED')
                    self.assertEqual(row['status']['gameplayAbility'], 'NOT_AUTHORED')
                    self.assertIsNone(row['imageConcept']['actualNiagaraAssetPath'])

    def test_05_index_and_preview_approve_names_only(self):
        for item in self.index['heroRecords']:
            hero = self.heroes[item['heroId']]
            self.assertEqual(item['names'], [r['nameZh'] for r in hero['skillRows']])
            self.assertEqual(hero['reviewUltimateName'], hero['skillRows'][3]['nameZh'])
        preview = data(V3 / 'CharacterCreatePreviewContract.json')
        self.assertEqual(preview['approvalGate']['skillName'], 'APPROVED_USER_SIMPLE_V5')
        self.assertEqual(preview['displayNameApprovalSource'], 'Docs/Design/ApprovedSkillDisplayNames_V5.json')
        self.assertEqual(preview['approvalGate']['visualEffect'], 'NOT_APPROVED')
        self.assertTrue(all(not x['visibleInProductionCreation'] for x in preview['heroDisplaySources']))
        self.assertEqual(self.index['approvedSkillNameCount'], 96)
        self.assertFalse(self.index['readyForCreationScreen'])
        self.assertEqual(self.index['approvedEffectImageCount'], 0)

    def test_06_changes_do_not_rewrite_stable_ids_or_balance(self):
        v2rows = {r['abilityId']: r for h in self.v2['heroes'] for r in h['slots']}
        for hero in self.heroes.values():
            for row in hero['skillRows']:
                old = v2rows[row['abilityId']]
                self.assertEqual(row['slotId'], old['slotId'])
                self.assertEqual(row['displayOrder'], old['displayOrder'])
                for a, b in [('baseDamage','baseDamage'),('hitCount','hitCount'),('cooldownSeconds','cooldownSeconds'),('rangeCm','castRangeCm'),('radiusCm','areaRadiusCm'),('momentumCost','momentumCost')]:
                    self.assertEqual(row['balanceV2Reference'][a], old[b])

    def migration_module(self):
        path = ROOT / 'Tools/Unreal/Abilities/ApplyApprovedSkillDisplayNames.py'
        spec = importlib.util.spec_from_file_location('approved_name_migration', path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    def test_07_repeat_migration_is_idempotent(self):
        module = self.migration_module()
        _, before, after, changes = module.collect()
        self.assertEqual(before, after)
        self.assertEqual(changes, {})

    def test_08_unapproved_concurrent_name_is_rejected_before_writes(self):
        module = self.migration_module()
        originals = {path: data(ROOT / path) for path in module.ALLOWED_PATHS}
        originals[module.HERO_PATHS[0]]['skillRows'][0]['nameZh'] = '未授权名'
        with self.assertRaises(ValueError):
            module.transform(originals, module.registry())

    def test_09_invariant_projection_covers_mechanics_values_images_and_ids(self):
        module = self.migration_module()
        path = module.HERO_PATHS[0]
        original = data(ROOT / path)
        invariant = module.invariant_projection(path, original)
        for key in ('mechanics', 'effectImage', 'gameplayAbility', 'canShowInProductionCreation'):
            modified = copy.deepcopy(original)
            modified['skillRows'][0]['status'][key] = 'WRONGLY_APPROVED'
            self.assertNotEqual(module.invariant_projection(path, modified), invariant)
        modified = copy.deepcopy(original)
        modified['skillRows'][0]['balanceV2Reference']['baseDamage'] += 1
        self.assertNotEqual(module.invariant_projection(path, modified), invariant)
        modified = copy.deepcopy(original)
        modified['skillRows'][0]['abilityId'] = 'dba.ability.wrong@1'
        self.assertNotEqual(module.invariant_projection(path, modified), invariant)


if __name__ == '__main__':
    unittest.main(verbosity=2)
