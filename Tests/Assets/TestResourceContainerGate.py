"""合成IoStore夹具只验证发布策略；不代替真实Cook。"""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]


class ResourceContainerGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec=importlib.util.spec_from_file_location('resource_gate',ROOT/'Tools/Unreal/Resources/ValidateResourceOwnershipContainer.py')
        cls.api=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.api)

    def check(self,profile,extra=(),omit=None,bulk=False):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'Container.csv'
            groups=('plants_runtime','terrain_color') if profile=='VillageDevelopmentClient' else ()
            names=[m['target'] for m in self.api.DOC['moves'] if m['group'] in groups and m['target']!=omit]
            names += list(extra)
            if not names:names=['/DBAWorldPack_Village/Maps/L_Village_Start']
            with path.open('w',encoding='utf-8',newline='') as handle:
                writer=csv.writer(handle);writer.writerow(('Filename','ChunkType','Size','Platform'))
                for name in names:writer.writerow((name+'.uasset','BulkData' if bulk else 'ExportBundleData','1024','Windows'+self.api.PROFILES.get(profile,'Client')))
            return self.api.validate(path,profile)

    def test_clean_runtime_requires_61_unique_real_exports(self):
        self.assertTrue(self.check('VillageDevelopmentClient')['Passed'])
        self.assertFalse(self.check('VillageDevelopmentClient',omit='/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage/Materials/MaterialCollection')['Passed'])
        with self.assertRaises(ValueError):self.check('VillageDevelopmentClient',bulk=True)

    def test_all_profiles_reject_retired_and_new_history_or_vendor_paths(self):
        for profile in ('FrontEndClient','VillageDevelopmentClient','PluginDemoClient','VillageDevelopmentServer','FoundationStandalone'):
            for path in ('/Game/PN_OpenWorldFoliage/Maps/Showroom','/Game/StyleHex_Studio/Old',
                '/DBAPresentationPack_Core/Materials/Terrain/M_DBA_Terrain_Production10',
                '/Game/Development/Vendor/PN_OpenWorldFoliage/Maps/Showroom',
                '/DBAPresentationPack_Core/Development/History/BirdFlock/Meshes/Old'):
                with self.subTest(profile=profile,path=path):self.assertFalse(self.check(profile,[path])['Passed'])

    def test_demo_exception_is_only_reviewed_terrain_leaf(self):
        path='/DBAWorldPack_Village/Development/Terrain/Production10/M_DBA_Terrain_Production10'
        self.assertTrue(self.check('PluginDemoClient',[path])['Passed'])
        self.assertFalse(self.check('FrontEndClient',[path])['Passed'])
        self.assertFalse(self.check('VillageDevelopmentServer',[path])['Passed'])
        self.assertFalse(self.check('PluginDemoClient',['/DBAWorldPack_Village/Development/Terrain/Unreviewed'])['Passed'])

    def test_duplicate_export_and_metadata_cannot_claim_presence(self):
        path='/DBAFoliagePack_Core/Vendor/PN_OpenWorldFoliage/Materials/MaterialCollection'
        self.assertFalse(self.check('VillageDevelopmentClient',[path])['Passed'])
        with self.assertRaises(ValueError):self.check('UnknownClient')

    def test_both_cook_entries_enforce_current_resource_contract(self):
        for entry in ('CookFrontEndClient.ps1','CookFoundation.ps1'):
            text=(ROOT/'Build/Game'/entry).read_text(encoding='utf-8')
            self.assertIn('ValidateResourceOwnershipContainer.py',text)
            self.assertIn('ResourceOwnershipGateResult',text)

    def test_unapproved_contract_root_omission_cannot_silently_weaken_gate(self):
        import copy
        doc=copy.deepcopy(self.api.DOC);doc['retired_roots']=[]
        with self.assertRaises(ValueError):self.api.contract.validate_contract(doc)

    def test_native_shader_metadata_is_not_an_asset_or_required_export(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'Native.csv'
            with path.open('w',newline='',encoding='utf-8') as handle:
                writer=csv.writer(handle);writer.writerow(('Filename','ChunkType','Size','Platform'))
                writer.writerow(('<ShaderCode>','ShaderCode',1024,'WindowsClient'))
                writer.writerow(('/Engine/Content/BasicShapes/Cube.uasset','ExportBundleData',1024,'WindowsClient'))
            self.assertTrue(self.api.validate(path,'FrontEndClient')['Passed'])
            with path.open('a',newline='',encoding='utf-8') as handle:
                csv.writer(handle).writerow(('/Game/PN_OpenWorldFoliage/Bad.uasset','ShaderCode',1024,'WindowsClient'))
            self.assertFalse(self.api.validate(path,'FrontEndClient')['Passed'])


if __name__=='__main__':unittest.main()
