"""离线工具边界回归；CSV均为明确测试夹具，不是实际Cook/UE通过证据。"""
import csv
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Tools/Unreal/Weather'))
import PrepareUnifiedVegetationWindConfig as config
import UnifiedVegetationWindContract as wind_contract

spec = importlib.util.spec_from_file_location('wind_container', ROOT / 'Tests/Assets/ValidateUnifiedVegetationWindContainer.py')
container = importlib.util.module_from_spec(spec); spec.loader.exec_module(container)


class WindConfigTests(unittest.TestCase):
    def test_partial_replace_failure_rolls_back_only_owned_content(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder); a, b = root / 'a.ini', root / 'b.ini'
            a.write_bytes(b'before-a'); b.write_bytes(b'before-b')
            replace = config.os.replace
            def fail_second(source, target):
                if pathlib.Path(target) == b:
                    raise OSError('replace rejected')
                return replace(source, target)
            with patch.object(config.os, 'replace', side_effect=fail_second):
                with self.assertRaisesRegex(OSError, 'replace rejected'):
                    config.commit_updates(root, [(a, b'before-a', b'after-a'), (b, b'before-b', b'after-b')], root / 'backup')
            self.assertEqual(a.read_bytes(), b'before-a'); self.assertEqual(b.read_bytes(), b'before-b')
            self.assertFalse(list(root.glob('*.tmp')))

    def test_apply_preflight_failure_leaves_configuration_unchanged(self):
        sys.path.insert(0, str(ROOT / 'Build/Game'))
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder); ini = root / 'Game/Config/DefaultGame.ini'
            policy = root / 'Build/Game/ServerPresentationExclusions.json'
            ini.parent.mkdir(parents=True); policy.parent.mkdir(parents=True)
            before_ini, before_policy = b'[Other]\nKey=keep\n', b'{"CookDirectories":[]}'
            ini.write_bytes(before_ini); policy.write_bytes(before_policy)
            with patch('sys.argv', ['prepare', '--workspace', folder, '--apply']), patch(
                    'ServerPresentationPolicy.load_policy', side_effect=ValueError('preflight rejected')):
                with self.assertRaisesRegex(ValueError, 'preflight rejected'):
                    config.main()
            self.assertEqual(ini.read_bytes(), before_ini)
            self.assertEqual(policy.read_bytes(), before_policy)
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


class WindMPCContractTests(unittest.TestCase):
    def test_renamed_rain_and_duplicate_names_cannot_pass_by_count(self):
        scalars = list(wind_contract.SCALAR_DEFAULTS.items())
        vectors = [(name, (0., 0., 0., 0.)) for name in wind_contract.WIND_VECTORS]
        self.assertEqual(wind_contract.validate_mpc_records(scalars, vectors), [])
        wrong = [('Extra' if name == 'GP_Surface_RainIntensity' else name, value) for name, value in scalars]
        self.assertTrue(wind_contract.validate_mpc_records(wrong, vectors))
        self.assertTrue(wind_contract.validate_mpc_records(scalars + [scalars[0]], vectors))
        self.assertTrue(wind_contract.validate_mpc_records(scalars, vectors + [vectors[0]]))
    def test_wrong_original_default_cannot_pass(self):
        scalars = [(name, 0. if name == 'GP_Surface_GlobalMossInfluence' else value)
                   for name, value in wind_contract.SCALAR_DEFAULTS.items()]
        self.assertTrue(wind_contract.validate_mpc_records(scalars, [(name, (0., 0., 0., 0.)) for name in wind_contract.WIND_VECTORS]))


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
    def test_native_unreal_csv_has_spaces_after_commas(self):
        path = self.root / 'native.csv'
        path.write_text('OrderInContainer, Filename, ChunkType, Size\n0, ../../../Plugins/A.uasset, ExportBundleData, 128\n', encoding='utf-8')
        self.assertEqual(container.inventory(path), {'../../../Plugins/A.uasset'})
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
    def test_unreferenced_library_defaults_do_not_force_cook_but_runtime_material_does(self):
        client, server, cold, bindings = self.fixture()
        runtime = '/DBAFoliagePack_Core/Foliage/Trees/WeepingWillow/MI_DBA_Willow_Bark'
        unused = '/DBAFoliagePack_Core/Foliage/Materials/MI_DBA_Foliage_Prototype_Bark'
        cold.write_text(json.dumps({'stage': 'ColdReload', 'status': 'PASS', 'runtime_materials': [runtime]}))
        bindings.write_text(json.dumps({'materials': [runtime, unused], 'prototype_masters': [], 'weather_activation_pending': False}))
        with client.open('a') as target:
            target.write('../../../Plugins/' + container.suffix(runtime) + ',ExportBundleData,100\n')
        self.assertEqual(container.inspect(client, server, cold, bindings)['status'], 'PASS')
        client.write_text('\n'.join(line for line in client.read_text().splitlines() if 'MI_DBA_Willow_Bark' not in line))
        self.assertEqual(container.inspect(client, server, cold, bindings)['status'], 'FAIL')
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
