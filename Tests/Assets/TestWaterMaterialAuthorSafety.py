"""作者脏包回归：隔离反射桩在任何引擎写入前失败，不接触真实UE资产。"""
import importlib.util
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT/'Tools/Unreal/World/WaterUnderwaterMaterialsMonolith.py'


class WaterAuthorSafetyTests(unittest.TestCase):
    def load(self, dirty):
        assets = SimpleNamespace(does_asset_exist=lambda _: True,
            get_metadata_tag=lambda _, key: 'Complete' if key.endswith('GraphState') else 'VillageWaterSystems20261031',
            save_loaded_asset=Mock(side_effect=AssertionError('不得保存脏包')))
        fake = SimpleNamespace(Paths=SimpleNamespace(project_dir=lambda: str(ROOT/'Game')),
            EditorAssetLibrary=assets, MaterialEditingLibrary=SimpleNamespace(),
            EditorLoadingAndSavingUtils=SimpleNamespace(get_dirty_content_packages=lambda: [SimpleNamespace(get_name=lambda: name) for name in dirty]),
            load_asset=lambda _: object())
        spec = importlib.util.spec_from_file_location('water_material_safety', SCRIPT)
        module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, unreal=fake):
            spec.loader.exec_module(module)
        helper = SimpleNamespace(VILLAGE='/DBAWorldPack_Village/Maps/L_Village_Start', backup=Mock(),
            world_actors=lambda: (SimpleNamespace(get_path_name=lambda: '/DBAWorldPack_Village/Maps/L_Village_Start.L_Village_Start'), []))
        module.common=lambda: helper
        return module, helper

    def test_complete_master_dirty_is_rejected_before_early_return(self):
        module, _ = self.load(['/GamePlatformSurface/Materials/Water/M_GP_Underwater'])
        with self.assertRaisesRegex(RuntimeError, '未保存'):
            module.ensure_master()

    def test_instance_dirty_is_rejected_before_master_or_asset_writes(self):
        for package in ('/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_Underwater',
                        '/DBAWorldPack_Village/Water/Textures/T_DBA_Village_LakeMask'):
            module, helper = self.load([package])
            module.ensure_master=Mock(side_effect=AssertionError('脏包检查必须先于母材质作者'))
            with self.subTest(package=package), self.assertRaisesRegex(RuntimeError, '未保存'):
                module.author_instance()
            helper.backup.assert_not_called()


if __name__ == '__main__':
    unittest.main()
