"""非UE合同回归；合成容器/日志只是门禁夹具，不证明真实资产或Cook成功。"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
CONTRACT = ROOT / 'Tools/Unreal/VFX/SpellProjectiles/PresentationManifest.json'
sys.path.insert(0, str(CONTRACT.parent))


class SpellPresentationContractTest(unittest.TestCase):
    def test_nine_unique_current_bindings_and_stable_ids(self):
        data = json.loads(CONTRACT.read_text('utf-8'))
        bindings = data['bindings']
        self.assertEqual(9, len(bindings))
        self.assertEqual(9, len({v['definition'] for v in bindings}))
        for row in bindings:
            self.assertEqual('/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_' + row['spell'] + row['stage'], row['system'])
            self.assertEqual('presentation.dba.development.magic.' + row['spell'].lower() + '.' + row['stage'].lower() + '@1', row['logical_id'])

    def test_retired_authors_fail_without_unreal_or_mutation(self):
        for row in json.loads(CONTRACT.read_text('utf-8'))['retired_authors']:
            if row['mode'] != 'all':
                continue
            entry = ROOT / row['path']
            # 先以静态断言阻止旧活动作者执行，RED阶段也不能连接或改动编辑器。
            self.assertIn('RETIRED_ENTRY_POINT', entry.read_text('utf-8'))
            result = subprocess.run([sys.executable, '-B', str(entry), '--apply'], capture_output=True, text=True)
            self.assertNotEqual(0, result.returncode, row['path'])
            self.assertIn('RETIRED_ENTRY_POINT', result.stderr + result.stdout)
            self.assertNotIn('ModuleNotFoundError', result.stderr)

    def test_cook_config_is_generated_from_contract(self):
        from SpellPresentationContract import cook_config_block
        for profile in ('FrontEndClient', 'VillageDevelopmentClient'):
            source = (ROOT / 'Game/Config/Custom' / profile / 'DefaultGame.ini').read_text('utf-8-sig')
            self.assertIn(cook_config_block(), source)
            self.assertIn('bIsEditorOnly=False,Directories=((Path="/Game")),SpecificAssets=("/DBAPresentationPack_Core/VFX/SpellProjectiles/DA_DBA_SpellPresentationCook.DA_DBA_SpellPresentationCook")', source)
            for directory in ('DevelopmentMagic', 'IceArrow'):
                self.assertNotIn('+DirectoriesToAlwaysCook=(Path="/DBAPresentationPack_Core/VFX/' + directory + '")', source)

    def test_container_rejects_retired_system_even_with_all_required_assets(self):
        from SpellPresentationContract import load_manifest, container_required_assets, forbidden_systems, container_asset
        from ValidateDevelopmentCastingContainer import validate_exports, required_assets
        data = load_manifest()
        required = required_assets()
        passed = validate_exports({'../../../Plugins/' + p.lower() for p in required})
        self.assertEqual([], passed['Missing'])
        self.assertEqual([], passed['Forbidden'])
        legacy = container_asset(forbidden_systems(data)[0]).lower()
        rejected = validate_exports({'../../../Plugins/' + p.lower() for p in required} | {'../../../Plugins/' + legacy})
        self.assertIn(legacy, [v.lower() for v in rejected['Forbidden']])

    def test_cast_duration_schema_is_required(self):
        data = json.loads(CONTRACT.read_text('utf-8'))
        self.assertEqual({'User.CastDurationSec', 'User.ElapsedAtStartSec', 'User.HandSideSign'}, set(data['cast_parameters']))
        for row in data['bindings']:
            if row['stage'] == 'Cast':
                self.assertTrue(row['require_cast_parameters'])

    def test_native_validator_rejects_optional_cast_rule_and_wrong_binding(self):
        """unreal stub仅验证只读门禁拒绝行为，不运行/替代原生UE资产、编译或Cook。"""
        from SpellPresentationContract import load_manifest, required_packages
        data = load_manifest()

        class Asset:
            def __init__(self, path, **properties):
                self.path, self.properties = path, properties
            def get_editor_property(self, key):
                return self.properties[key]
            def get_path_name(self):
                return self.path + '.' + self.path.rsplit('/', 1)[1]

        names = {'NiagaraSystem', 'PrimaryAssetLabel', 'GamePlatformVFXAttachedDefinition', 'GamePlatformVFXInstantDefinition'} | {r['unreal_class'] for r in data['resources'] + data['editor_resources']}
        classes = {name: type(name, (Asset,), {}) for name in names}
        objects = {}
        for row in data['resources'] + data['editor_resources']:
            objects[row['path']] = classes[row['unreal_class']](row['path'], srgb=row.get('srgb'))
        for path in data['auxiliary_systems'] + data['auxiliary_definitions']:
            objects[path] = classes['NiagaraSystem'](path)
        for row in data['bindings']:
            extent = row['fixed_extent'] or 1000
            low = SimpleNamespace(x=-extent, y=-extent, z=-extent)
            high = SimpleNamespace(x=extent, y=extent, z=extent)
            system = classes['NiagaraSystem'](row['system'], fixed_bounds=SimpleNamespace(min=low, max=high, is_valid=True))
            objects[row['system']] = system
            logical, version = row['logical_id'].split('@')
            namespace, name = logical.rsplit('.', 1)
            rules = [SimpleNamespace(name=key, type='FLOAT', required=True, min_value=v[0], max_value=v[1]) for key, v in data['cast_parameters'].items()]
            cls = classes['GamePlatformVFXInstantDefinition' if row['stage'] == 'Impact' else 'GamePlatformVFXAttachedDefinition']
            objects[row['definition']] = cls(row['definition'], logical_id=SimpleNamespace(namespace=namespace, name=name, logical_version=int(version)),
                niagara_system=system, quality_variants=[], platform_variants=[], parameter_schema=SimpleNamespace(rules=rules))
        objects[data['cook_label']] = classes['PrimaryAssetLabel'](data['cook_label'],
            explicit_assets=[objects[p] for p in required_packages() if p != data['cook_label']], rules=SimpleNamespace(cook_rule='ALWAYS_COOK'), is_runtime_label=False, label_assets_in_my_directory=False)
        stub = SimpleNamespace(**classes, load_asset=objects.get,
            Paths=SimpleNamespace(get_project_file_path=lambda: str(ROOT / 'Game/DivineBeastsArena.uproject')),
            SystemLibrary=SimpleNamespace(get_engine_version=lambda: '5.8.0'),
            PrimaryAssetCookRule=SimpleNamespace(ALWAYS_COOK='ALWAYS_COOK'),
            GamePlatformVFXParameterType=SimpleNamespace(FLOAT='FLOAT'))
        spec = importlib.util.spec_from_file_location('spell_native_validator_test_only', ROOT / 'Tests/Assets/ValidateSpellPresentationUE58.py')
        module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, unreal=stub):
            spec.loader.exec_module(module)
        self.assertTrue(module.validate(raise_on_error=False)['Passed'])
        row = next(r for r in data['bindings'] if r['stage'] == 'Cast')
        definition = objects[row['definition']]
        definition.properties['parameter_schema'].rules[0].required = False
        self.assertFalse(module.validate(raise_on_error=False)['Passed'])
        definition.properties['parameter_schema'].rules[0].required = True
        definition.properties['niagara_system'] = objects[data['bindings'][-1]['system']]
        self.assertFalse(module.validate(raise_on_error=False)['Passed'])


if __name__ == '__main__':
    unittest.main()
