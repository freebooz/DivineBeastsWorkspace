"""旧作者入口回归：仅源码/纯Python执行，前置静态断言确保RED也不连接UE。"""
import ast
import importlib.util
from pathlib import Path
import sys
import tempfile
import subprocess
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
AUTHOR = ROOT / 'Tools/Unreal/VFX/DevelopmentMagic/AuthorIceArrowNiagara.py'
sys.path.insert(0, str(ROOT / 'Tools/Unreal/VFX/SpellProjectiles'))


class RetiredSpellAuthorsTest(unittest.TestCase):
    def load_author(self):
        spec = importlib.util.spec_from_file_location('test_only_ice_author', AUTHOR)
        author = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(author)
        return author

    def assert_early_guards(self):
        tree = ast.parse(AUTHOR.read_text('utf-8'))
        functions = {v.name: v for v in tree.body if isinstance(v, ast.FunctionDef)}
        for name in ('main', 'author_one'):
            body = functions[name].body
            guards = [i for i, node in enumerate(body) if isinstance(node, ast.Expr) and isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Name) and node.value.func.id == 'guard_authoring']
            self.assertTrue(guards, name + ' must guard retired stages before any protocol or mutation')
            calls = [i for i, node in enumerate(body) if any(isinstance(n, ast.Call) and isinstance(n.func, ast.Name) and n.func.id in ('protocol', 'ensure_module') for n in ast.walk(node))]
            self.assertTrue(not calls or guards[0] < min(calls), name)
        return tree

    def test_default_and_retired_main_fail_before_protocol(self):
        self.assert_early_guards()  # RED在此失败，绝不执行尚未加guard的旧作者。
        author = self.load_author()
        for args in ([], ['--apply'], ['--apply', '--system', 'Cast'], ['--apply', '--system', 'Flight'], ['--apply', '--system', 'Impact']):
            with patch.object(sys, 'argv', [str(AUTHOR)] + args), patch.object(author, 'protocol', side_effect=AssertionError('protocol must not run')):
                with self.assertRaisesRegex(RuntimeError, 'RETIRED_ENTRY_POINT'):
                    author.main()

    def test_direct_author_one_fails_before_any_protocol_method(self):
        self.assert_early_guards()
        author = self.load_author()
        class ForbiddenProtocol:
            def __getattr__(self, name):
                raise AssertionError('protocol access before retired guard: ' + name)
        for stage in ('Cast', 'Flight', 'Impact'):
            with self.assertRaisesRegex(RuntimeError, 'RETIRED_ENTRY_POINT'):
                author.author_one(ForbiddenProtocol(), stage)

    def test_auxiliary_stages_and_read_only_hlsl_import_survive(self):
        self.assert_early_guards()
        author = self.load_author()
        self.assertIn('GPU_SIMULATION', author.HLSL)
        for stage in ('GroundRune', 'Launch', 'FrostResidue'):
            with patch.object(sys, 'argv', [str(AUTHOR), '--system', stage]), patch.object(author, 'protocol', side_effect=AssertionError('inspect is read only')):
                author.main()
            token = object()
            with patch.object(sys, 'argv', [str(AUTHOR), '--apply', '--system', stage]), patch.object(author, 'protocol', return_value=token), patch.object(author, 'ensure_module') as module_step, patch.object(author, 'author_one') as stage_step:
                author.main()
                module_step.assert_called_once_with(token)
                stage_step.assert_called_once_with(token, stage)
        from SpellPresentationContract import guard_authoring
        guard_authoring(AUTHOR, ['GroundRune', 'Launch', 'FrostResidue'])

    def test_ground_cast_refiner_is_explicitly_retired_before_importing_protocol(self):
        entry = ROOT / 'Tools/Unreal/VFX/DevelopmentMagic/RefineIceArrowGroundCastMonolith.py'
        source = entry.read_text('utf-8')
        self.assertIn('RETIRED_ENTRY_POINT', source)
        self.assertNotIn('set_editor_property', source)

    def test_old_hotbar_binding_fails_before_unreal_import(self):
        """RED先静态拒绝活动旧脚本，绝不执行它的顶层导入/目录创建/UE写入。"""
        entry = ROOT / 'Tools/Unreal/Abilities/BindDirectedMissileSlot1HealingSlot5UE.py'
        source = entry.read_text('utf-8')
        self.assertTrue('RETIRED_ENTRY_POINT' in source, 'old hotbar writer must be explicitly retired')
        self.assertNotIn('set_editor_property', source)
        self.assertNotIn('import unreal', source)
        for args in ([], ['--apply']):
            result = subprocess.run([sys.executable, '-B', str(entry)] + args, capture_output=True, text=True)
            self.assertNotEqual(0, result.returncode)
            self.assertIn('RETIRED_ENTRY_POINT', result.stderr)
            self.assertIn('Tools/Unreal/Abilities/BindFireIceHotbarUE58.py', result.stderr)
            self.assertNotIn('ModuleNotFoundError', result.stderr)
        spec = importlib.util.spec_from_file_location('retired_hotbar_import_test_only', entry)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)  # 兼容导入不依赖unreal，不执行旧作者顶层目录创建。
        for name in ('run', 'main', 'apply'):
            with self.assertRaisesRegex(RuntimeError, 'BindFireIceHotbarUE58.py'):
                getattr(module, name)()

    def test_hotbar_replacement_is_current_author_without_changing_approved_keys(self):
        from SpellPresentationContract import load_manifest
        relative = 'Tools/Unreal/Abilities/BindDirectedMissileSlot1HealingSlot5UE.py'
        matches = [v for v in load_manifest()['retired_authors'] if v['path'] == relative]
        self.assertEqual(1, len(matches), 'old hotbar author must be registered in the single retirement contract')
        self.assertEqual('all', matches[0]['mode'])
        self.assertEqual('Tools/Unreal/Abilities/BindFireIceHotbarUE58.py', matches[0]['replacement'])
        tree = ast.parse((ROOT / matches[0]['replacement']).read_text('utf-8'))
        new = next(v.value for v in tree.body if isinstance(v, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'NEW' for t in v.targets))
        self.assertEqual({'fireball': 'Primary', 'frostbolt': 'Slot1', 'directedmissile': 'Slot2'}, ast.literal_eval(new))

    def test_migration_source_candidates_reject_localized_and_external_packages(self):
        from SpellPresentationContract import inspect_migration_source_candidates
        scratch = ROOT / 'Saved/Validation/SingleImplementationConvergence20261027/MigrationPreflightFixtures'
        scratch.mkdir(parents=True, exist_ok=True)
        # 使用明确fixture扩展名；不生成文本假.uasset，也不调用UE。
        for relative in ('L10N/fr/VFX/MyAsset.fixture', '__ExternalObjects__/hashed/Package.fixture'):
            with tempfile.TemporaryDirectory(dir=scratch) as folder:
                content = Path(folder) / 'Content'
                content.mkdir()
                candidate = content / relative
                candidate.parent.mkdir(parents=True)
                candidate.write_text('test-only-package-candidate', 'utf-8')
                with self.assertRaisesRegex(RuntimeError, '候选非空'):
                    inspect_migration_source_candidates(['/TestMount/VFX/MyAsset'], {'TestMount': content}, [])

    def test_migration_candidate_evidence_is_limited_and_registry_external_fails(self):
        from SpellPresentationContract import inspect_migration_source_candidates
        scratch = ROOT / 'Saved/Validation/SingleImplementationConvergence20261027/MigrationPreflightFixtures'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            content = Path(folder) / 'Content'
            content.mkdir()
            report = inspect_migration_source_candidates(['/TestMount/VFX/MyAsset'], {'TestMount': content}, [])
            self.assertFalse(report['RuntimeExternalPackagesQueried'])
            self.assertEqual([], report['ExternalDiskCandidates'])
            with self.assertRaisesRegex(RuntimeError, '候选非空'):
                inspect_migration_source_candidates(['/TestMount/VFX/MyAsset'], {'TestMount': content}, ['/TestMount/__ExternalActors__/Hash'])


if __name__ == '__main__':
    unittest.main()
