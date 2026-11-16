"""效果场景的真实编目路由回归；不代替关卡加载或人工视听核验。"""
from pathlib import Path
import json
import unittest

ROOT = Path(__file__).resolve().parents[2]
HUB = '/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub'


class PluginVisualSceneRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ROOT / 'Docs/Implementation/PluginDemos/PluginDemoManifest.json').read_text(encoding='utf-8-sig'))
        cls.scenarios = cls.manifest['scenarios']
        cls.by_feature = {s['feature_id']: s for s in cls.scenarios}
        cls.mounts = {'Game': ROOT / 'Game/Content'}
        cls.mounts.update({p['plugin_id']: (ROOT / p['descriptor_path']).parent / 'Content' for p in cls.manifest['plugins']})

    def test_vfx_uses_real_review_level(self):
        for feature in ('ten_behaviors', 'preload', 'quality_prediction', 'dissolve'):
            self.assertEqual('/GamePlatformVFX/Development/Review/Maps/L_VFXReview',
                             self.by_feature['GamePlatformVFX.' + feature]['map_asset'])

    def test_terrain_and_surface_marks_have_specific_scenes(self):
        self.assertEqual('/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview',
                         self.by_feature['GamePlatformSurface.environment']['map_asset'])
        self.assertEqual('/GamePlatformSurface/Development/Review/Maps/L_GP_SurfaceMarksReview',
                         self.by_feature['GamePlatformSurface.marks']['map_asset'])

    def test_water_does_not_route_to_empty_same_name_candidate(self):
        for feature in ('sampling', 'presence', 'visual'):
            self.assertEqual('/Game/Development/Water/L_GP_WaterReview',
                             self.by_feature['GamePlatformWater.' + feature]['map_asset'])

    def test_asset_gallery_uses_observable_level(self):
        for scenario in self.scenarios:
            if scenario['adapter_id'] == 'ContentAsset':
                self.assertEqual('/Game/Development/PluginDemos/Maps/L_DBA_AssetGallery', scenario['map_asset'])

    def test_editor_fixtures_keep_exact_hub_qualification(self):
        for scenario in self.scenarios:
            if (scenario['adapter_id'].startswith('EditorManual.') or scenario['feature_id'] in
                    ('GamePlatformPCG.authoring', 'GamePlatformPCG.spatial_bake', 'GamePlatformSurface.editor_wind')):
                self.assertEqual(HUB, scenario['map_asset'])

    def test_pcg_world_inputs_keep_the_matching_authorized_host(self):
        # ValidateProfile 与 Generate 都读取真实 World readiness；仅有 Landscape 不能替代宿主定义。
        for feature in ('generation', 'inspection'):
            self.assertEqual(HUB, self.by_feature['GamePlatformPCG.' + feature]['map_asset'])

    def test_every_nonempty_map_reference_exists_in_its_real_mount(self):
        for scenario in self.scenarios:
            package = scenario['map_asset'].split('.')[0]
            if package:
                mount, *parts = package.strip('/').split('/')
                self.assertIn(mount, self.mounts)
                self.assertTrue(self.mounts[mount].joinpath(*parts).with_suffix('.umap').is_file(), package)


if __name__ == '__main__':
    unittest.main(verbosity=2)
