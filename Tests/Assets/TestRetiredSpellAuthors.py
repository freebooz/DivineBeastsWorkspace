"""旧作者入口回归：仅源码/纯Python执行，前置静态断言确保RED也不连接UE。"""
import ast
import importlib.util
from pathlib import Path
import sys
import tempfile
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
        from SpellPresentationContract import guard_authoring
        guard_authoring(AUTHOR, ['GroundRune', 'Launch', 'FrostResidue'])

    def test_ground_cast_refiner_is_explicitly_retired_before_importing_protocol(self):
        entry = ROOT / 'Tools/Unreal/VFX/DevelopmentMagic/RefineIceArrowGroundCastMonolith.py'
        source = entry.read_text('utf-8')
        self.assertIn('RETIRED_ENTRY_POINT', source)
        self.assertNotIn('set_editor_property', source)

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
