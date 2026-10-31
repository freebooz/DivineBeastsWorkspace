"""资源迁移合同回归：拒绝越界、冲突和覆盖，测试不接触真实编辑器。"""
import copy
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT/'Tools/Unreal/Resources/ResourceOwnershipContract.py'


class ResourceOwnershipContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('resource_ownership', MODULE)
        cls.api = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.api)

    def document(self):
        return self.api.load_contract()

    def test_authorized_inventory_has_exact_owner_split(self):
        doc = self.document()
        moves = self.api.validate_contract(doc)
        self.assertEqual(len(moves), 175)
        counts = {g: sum(x['group'] == g for x in moves) for g in self.api.GROUPS}
        self.assertEqual(counts, {'plants_runtime':51, 'plants_samples':40,
            'terrain_review':12, 'terrain_color':10, 'bird_history':7, 'stylehex_samples':55})
        self.assertFalse(any('/Meshes/Flight/' in x['source'] for x in moves))

    def test_duplicate_case_insensitive_target_is_rejected(self):
        doc = self.document()
        doc['moves'][1]['target'] = doc['moves'][0]['target'].swapcase()
        with self.assertRaisesRegex(ValueError, '重复|冲突'):
            self.api.validate_contract(doc)

    def test_path_escape_and_unapproved_owner_are_rejected(self):
        for suffix in ('/../Other', '/Bad.Name', '/With Space', '/X;Y'):
            doc = self.document(); doc['moves'][0]['target'] += suffix
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                self.api.validate_contract(doc)
        doc = self.document(); doc['moves'][0]['target']='/GamePlatformVFX/Vendor/Foreign'
        with self.assertRaisesRegex(ValueError, '归属|Owner'):
            self.api.validate_contract(doc)

    def test_preexisting_destination_is_never_overwritten(self):
        doc = self.document(); move = doc['moves'][0]
        with self.assertRaisesRegex(ValueError, '目标已存在'):
            self.api.preflight_paths(doc, {move['source']}, {move['target']})

    def test_old_vendor_and_public_world_dependencies_block_release(self):
        doc = self.document()
        edges=[('/DBAFoliagePack_Core/Foliage/Grass/LGT', '/Game/PN_OpenWorldFoliage/Meshes/Old'),
            ('/DBAPresentationPack_Core/Materials/Master', '/DBAWorldPack_Village/Terrain/Textures/T')]
        issues=self.api.ownership_issues(doc, edges)
        self.assertEqual(len(issues), 2)
        self.assertEqual(self.api.ownership_issues(doc, [('/DBAWorldPack_Village/Maps/L',
            '/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage/Meshes/Tree')]), [])

    def test_texture_streaming_size_is_not_source_image_size(self):
        before={'class':'/Script/Engine.Texture2D','size':[32,32], 'pixelPNG':'same-source', 'srgb':True}
        after={**before,'size':[512,512]}
        self.assertEqual(self.api.appearance_signature(before), self.api.appearance_signature(after))
        self.assertNotEqual(self.api.appearance_signature(before),
            self.api.appearance_signature({**after,'pixelPNG':'changed-source'}))

    def test_native_fname_leaf_casing_does_not_change_identity(self):
        self.assertEqual(self.api.appearance_signature({'leaf':'ground_ao'}),
            self.api.appearance_signature({'leaf':'ground_AO'}))

    def test_retired_bird_author_cannot_recreate_old_runtime_meshes(self):
        self.assertFalse((ROOT/'Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockAssets.py').exists())
        import runpy
        with self.assertRaisesRegex(RuntimeError,'已退休'):
            runpy.run_path(str(ROOT/'Tools/Archive/Resources/20261031/AuthorBirdFlockAssets.py'),run_name='__main__')

    def test_completed_recovery_and_legacy_validator_are_outside_active_tools(self):
        for path in ('Tools/Unreal/Terrain/ResumeTerrainV3FormalCover.ps1','Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockAssets.py'):
            self.assertFalse((ROOT/path).exists())
            self.assertTrue((ROOT/'Tools/Archive/Resources/20261031'/Path(path).name).is_file())

    def test_project_archives_reject_before_unreal_import_or_callback_registration(self):
        import json,runpy
        rows=json.loads((ROOT/'Tools/Archive/ScriptRetirementRegistry_20261031.json').read_text(encoding='utf-8'))['scripts']
        self.assertEqual(len(rows),7)
        for row in rows:
            self.assertFalse((ROOT/row['source']).exists())
            if row['archive'].endswith('.py'):
                with self.subTest(path=row['archive']),self.assertRaisesRegex(RuntimeError,'已退休'):
                    runpy.run_path(str(ROOT/row['archive']),run_name='__main__')


if __name__ == '__main__':
    unittest.main()
