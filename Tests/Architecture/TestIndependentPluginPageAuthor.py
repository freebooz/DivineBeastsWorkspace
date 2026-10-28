"""页作者计划的输入边界回归；离线检查不代表原生UMG已经生成。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py'


class IndependentPageAuthorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('independent_page_author', TOOL)
        cls.tool = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.tool)
        cls.document = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))

    def test_plan_has_one_fixed_page_per_actual_plugin(self):
        plan = self.tool.make_page_plan(self.document)
        actual = {p.stem for p in (ROOT / 'Game/Plugins').rglob('*.uplugin')}
        self.assertEqual(actual, {p['plugin_id'] for p in plan})
        for page in plan:
            self.assertEqual(page['plugin_id'], page['fixed_plugin_id'])
            self.assertTrue(page['widget_class'].endswith('_' + page['plugin_id'] + '_C'))
        self.assertEqual({self.tool.HUB_DEFINITION}, {p['definition_template'] for p in plan})

    def test_alias_rejected_before_engine_import_or_write(self):
        document = copy.deepcopy(self.document)
        document['plugins'][0]['test_page_asset'] = document['plugins'][1]['test_page_asset']
        with self.assertRaisesRegex(ValueError, 'page identity'):
            self.tool.make_page_plan(document)

    def test_duplicate_or_unknown_plugin_selection_rejected(self):
        identity = self.document['plugins'][0]['plugin_id']
        for selected in [[identity, identity], ['NotARegisteredPlugin']]:
            with self.assertRaises(ValueError):
                self.tool.make_page_plan(self.document, selected)

    def test_legacy_manifest_cannot_silently_author_default_context_pages(self):
        document = copy.deepcopy(self.document)
        document.pop('manual_page_contract_version')
        with self.assertRaisesRegex(ValueError, 'manual_page_contract_version'):
            self.tool.make_page_plan(document)

    def test_saved_snapshot_does_not_claim_cold_reload_or_human_pass(self):
        result = self.tool.authoring_result([], 'sha')
        self.assertEqual('not_run', result['cold_reload'])
        self.assertEqual('not_run', result['runtime'])
        self.assertEqual('pending', result['human_review'])

    def test_cold_reload_requires_process_started_after_saved_assets(self):
        self.assertFalse(self.tool.is_cold_reload(10, 200.0, 10, 250.0))
        self.assertFalse(self.tool.is_cold_reload(10, 200.0, 11, 199.0))
        self.assertFalse(self.tool.is_cold_reload(10, 200.0, 11, 200.0))
        self.assertTrue(self.tool.is_cold_reload(10, 200.0, 11, 201.0))


if __name__ == '__main__':
    unittest.main()
