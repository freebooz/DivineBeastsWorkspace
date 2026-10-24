"""离线工具边界回归；CSV均为明确测试夹具，不是实际Cook/UE通过证据。"""
import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import PrepareUnifiedVegetationWindConfig as config

spec = importlib.util.spec_from_file_location('wind_container', ROOT / 'Tests/Assets/ValidateUnifiedVegetationWindContainer.py')
container = importlib.util.module_from_spec(spec); spec.loader.exec_module(container)


class WindConfigTests(unittest.TestCase):
    def test_preserves_other_worlds_and_is_idempotent(self):
        source = '[' + config.SECTION + ']\r\n' + config.PROPERTY + '=(("/Other/Maps/A","other.clear@1"))\r\n'
        result = config.configure_ini(source)
        self.assertIn('("/Other/Maps/A","other.clear@1")', result)
        self.assertIn(config.WORLD, result)
        self.assertEqual(config.configure_ini(result), result)
        self.assertEqual(result.count('+DirectoriesToNeverCook='), 1)
        self.assertIn('\r\n', result)

    def test_explicit_snow_choice_is_preserved_by_rejection(self):
        source = '[' + config.SECTION + ']\n' + config.PROPERTY + '=(("' + config.WORLD + '","db.weather.heavysnow@1"))\n'
        with self.assertRaisesRegex(ValueError, '明确天气选择'):
            config.configure_ini(source)

    def test_unknown_map_syntax_is_not_overwritten(self):
        source = '[' + config.SECTION + ']\n' + config.PROPERTY + '=(Unknown=1)\n'
        with self.assertRaises(ValueError):
            config.configure_ini(source)

    def test_ini_names_are_case_insensitive_and_whitespace_is_supported(self):
        source = '[' + config.SECTION.lower() + ']\n' + config.PROPERTY.lower() + ' = (("/Other/Maps/A","other.clear@1"))\n'
        result = config.configure_ini(source)
        self.assertEqual(result.lower().count(config.PROPERTY.lower()), 1)
        self.assertIn('("/Other/Maps/A","other.clear@1")', result)


class WindContainerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = pathlib.Path(self.temp.name)
    def tearDown(self):
        self.temp.cleanup()
    def csv(self, name, rows):
        path = self.root / name
        with path.open('w', newline='', encoding='utf-8') as target:
            writer = csv.writer(target); writer.writerow(['Filename', 'ChunkType', 'Size']); writer.writerows(rows)
        return path
    def test_metadata_cannot_prove_asset_delivery(self):
        path = self.csv('metadata.csv', [('Fake.uasset', 'PackageStoreEntry', '100')])
        with self.assertRaisesRegex(ValueError, 'ExportBundleData'):
            container.inventory(path)
    def test_zero_size_asset_is_rejected(self):
        path = self.csv('zero.csv', [('Fake.uasset', 'ExportBundleData', '0')])
        with self.assertRaisesRegex(ValueError, '尺寸非法'):
            container.inventory(path)
    def fixture(self):
        required = [container.contract.GLOBAL_FUNCTION, container.contract.GENERIC_FUNCTION,
                    container.contract.ST9_FUNCTION, container.contract.ST9_MASTER,
                    '/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal']
        client = self.csv('client.csv', [('../../../Plugins/' + container.suffix(p), 'ExportBundleData', '100') for p in required])
        server = self.csv('server.csv', [('../../../Plugins/DBAWorldPack_Village/Content/Weather/Definitions/DA_DBA_Weather_Clear.uasset', 'ExportBundleData', '100')])
        cold = self.root / 'cold.json'; cold.write_text(json.dumps({'stage': 'ColdReload', 'status': 'PASS'}))
        bindings = self.root / 'bindings.json'; bindings.write_text(json.dumps({'materials': [], 'prototype_masters': [], 'weather_activation_pending': False}))
        return client, server, cold, bindings
    def test_missing_runtime_master_is_detected(self):
        client, server, cold, bindings = self.fixture()
        text = client.read_text(); client.write_text('\n'.join(line for line in text.splitlines() if 'M_DBA_Foliage_ST9_Master' not in line))
        result = container.inspect(client, server, cold, bindings)
        self.assertEqual(result['status'], 'FAIL'); self.assertIn(container.contract.ST9_MASTER, result['client_missing'])
    def test_visual_leak_on_server_is_detected(self):
        client, server, cold, bindings = self.fixture()
        with server.open('a') as target:
            target.write('../../../Plugins/DBAFoliagePack_Core/Content/Foliage/Wind.uasset,ExportBundleData,100\n')
        self.assertEqual(container.inspect(client, server, cold, bindings)['status'], 'FAIL')
    def test_weather_activation_cannot_be_left_pending(self):
        client, server, cold, bindings = self.fixture()
        bindings.write_text(json.dumps({'materials': [], 'prototype_masters': [], 'weather_activation_pending': True}))
        with self.assertRaisesRegex(ValueError, '天气'):
            container.inspect(client, server, cold, bindings)


if __name__ == '__main__':
    unittest.main()
