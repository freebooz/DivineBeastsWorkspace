"""独立插件页作者合同回归；验证真实插件身份和路径归属，不把离线通过当UMG资产或人工验收。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('plugin_manifest_validator', Path(__file__).with_name('ValidatePluginDemoManifest.py'))
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
MANIFEST = ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json'


class IndependentPluginPageContracts(unittest.TestCase):
    def setUp(self):
        self.document = json.loads(MANIFEST.read_text(encoding='utf-8-sig'))

    def test_every_installed_plugin_is_addressable(self):
        actual = {p.stem for p in (ROOT / 'Game/Plugins').rglob('*.uplugin')}
        self.assertEqual(actual, {p['plugin_id'] for p in self.document['plugins']})
        self.assertEqual(1, self.document.get('manual_page_contract_version'))

    def test_page_identity_is_bound_to_its_plugin(self):
        pages = []
        for plugin in self.document['plugins']:
            expected = '/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_' + plugin['plugin_id']
            self.assertEqual(expected, plugin.get('test_page_asset'), plugin['plugin_id'])
            pages.append(plugin['test_page_asset'])
        self.assertEqual(len(pages), len(set(pages)))

    def test_cross_plugin_page_alias_is_rejected(self):
        doc = copy.deepcopy(self.document)
        doc['manual_page_contract_version'] = 1
        for plugin in doc['plugins']:
            plugin['test_page_asset'] = '/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_' + plugin['plugin_id']
        doc['plugins'][0]['test_page_asset'] = doc['plugins'][1]['test_page_asset']
        issues = VALIDATOR.validate_manifest(doc, ROOT)
        self.assertTrue(any(i['code'] == 'manual_page.identity' for i in issues), issues)

    def test_project_external_asset_path_is_rejected(self):
        doc = copy.deepcopy(self.document)
        doc['manual_page_contract_version'] = 1
        for plugin in doc['plugins']:
            plugin['test_page_asset'] = '/DBAUIPack_Core/Development/PluginDemos/UI/Plugins/WBP_DBA_PluginTest_' + plugin['plugin_id']
        doc['plugins'][0]['test_page_asset'] = '/Game/External/../WBP_Test'
        issues = VALIDATOR.validate_manifest(doc, ROOT)
        self.assertTrue(any(i['code'] == 'manual_page.identity' for i in issues), issues)


if __name__ == '__main__':
    unittest.main()
