"""内容预览的资源路径必须独立于稳定标识/脚本文本；只验证作者合同，不加载UE。"""
import copy
import importlib.util
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('manifest_parameter_fixture', HERE / 'TestPluginDemoManifest.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)
spec = importlib.util.spec_from_file_location('manifest_asset_parameter_validator', HERE / 'ValidatePluginDemoManifest.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


class AssetParameterTests(unittest.TestCase):
    def document(self, value, kind='asset_path'):
        document = fixture.make_manifest()
        document['scenarios'][0]['parameters'] = [{'parameter_id': 'Asset', 'type': kind,
            'unit': '原生顶层对象路径', 'default_value': value, 'default_source': '实际AssetRegistry身份',
            'writable': True, 'apply_when': 'execute'}]
        return document

    def test_typed_native_asset_path_is_accepted(self):
        issues = validator.validate_manifest(self.document('/DBAHeroPack_Rat/UI/T_Icon.T_Icon'), fixture.WORKSPACE)
        self.assertEqual([], issues)

    def test_stable_identifier_does_not_become_arbitrary_path(self):
        issues = validator.validate_manifest(self.document('/DBAHeroPack_Rat/UI/T_Icon.T_Icon', 'identifier'), fixture.WORKSPACE)
        self.assertTrue(issues)

    def test_asset_payload_requires_string_and_explicit_object(self):
        for value in [0, True, None, '', '/Game/TextureOnly', 'None']:
            with self.subTest(value=value):
                issues = validator.validate_manifest(self.document(value), fixture.WORKSPACE)
                self.assertTrue(issues)

    def test_os_path_subobject_traversal_and_script_rejected(self):
        for value in ['C:/Temp/Test.png', '/Game/../Test.Test', '/Game/A.A:Inner', '/Content/A.A',
                      '/Game/A.A;quit', '/Game/A.A\n', '/Game/A.A$(quit)', '/Game/A.A/B']:
            with self.subTest(value=value):
                self.assertTrue(validator.validate_manifest(self.document(value), fixture.WORKSPACE))


if __name__ == '__main__':
    unittest.main()
