"""验证出生点允许装饰草，但水、道路、建筑和人工锁定仍被排除。"""
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
path = ROOT / 'Tools/Unreal/PCG/GenerateFabVillageGrassExclusion.py'
spec = importlib.util.spec_from_file_location('fab_grass_exclusion', path)
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class SpawnMaskTests(unittest.TestCase):
    def test_lake_projection_does_not_exclude_high_ground(self):
        heights = [[0.0 for _ in range(21)] for _ in range(21)]
        for y in range(5, 11):
            for x in range(5, 11):
                heights[y][x] = 500.0
        data = {'landscapeBounds': [0, 0, 2000, 2000],
                'origin': [0, 0], 'step': 100, 'nx': 21, 'ny': 21,
                'heights': heights, 'waterZ': 0,
                'lakePolygon': [[100, 100], [1100, 100], [1100, 1100], [100, 1100]],
                'riverPoints': [[100, 1600, 0, 100], [1900, 1600, 0, 100]],
                'spatialMasks': [{'domain': 'Gameplay.Spawn', 'vertices': [[500, 500], [1000, 500], [1000, 1000], [500, 1000]], 'closed': True, 'fill': True, 'halfWidth': 0}]}
        image, _ = generator.render_mask(data, size=200)
        self.assertEqual(image.getpixel((70, 70)), 0, '明显高于湖面的陆地应允许花草')
        self.assertEqual(image.getpixel((20, 20)), 255, '湖面覆盖的低地仍被排除')

    def test_spawn_is_open_but_overlapping_exclusions_remain(self):
        data = {'landscapeBounds': [0, 0, 2000, 2000],
                'origin': [0, 0], 'step': 100, 'nx': 21, 'ny': 21,
                'heights': [[0.0 for _ in range(21)] for _ in range(21)], 'waterZ': 0,
                'lakePolygon': [[0, 0], [200, 0], [200, 200], [0, 200]],
                'riverPoints': [[100, 1600, 0, 100], [1900, 1600, 0, 100]],
                'spatialMasks': [
                    {'domain': 'Gameplay.Spawn', 'vertices': [[400, 400], [1200, 400], [1200, 1200], [400, 1200]], 'closed': True, 'fill': True, 'halfWidth': 0},
                    {'domain': 'Road.Main', 'vertices': [[400, 800], [1200, 800]], 'closed': False, 'fill': False, 'halfWidth': 75},
                    {'domain': 'Building.Parcel', 'vertices': [[800, 950], [1000, 950], [1000, 1150], [800, 1150]], 'closed': True, 'fill': True, 'halfWidth': 0},
                    {'domain': 'ManualLock', 'vertices': [[1050, 450], [1150, 450], [1150, 550], [1050, 550]], 'closed': True, 'fill': True, 'halfWidth': 0},
                ]}
        image, included = generator.render_mask(data, size=200)
        pixel = lambda x, y: image.getpixel((round(x / 10), round(y / 10)))
        self.assertEqual(pixel(500, 500), 0, '出生区域的普通地面应允许花草')
        self.assertEqual(pixel(600, 800), 255, '出生区域内道路仍被排除')
        self.assertEqual(pixel(900, 1000), 255, '出生区域内建筑仍被排除')
        self.assertEqual(pixel(1100, 500), 255, '人工锁定仍被排除')
        self.assertEqual(pixel(100, 100), 255, '湖面仍被排除')
        self.assertEqual(pixel(1000, 1600), 255, '河道仍被排除')
        self.assertNotIn('Gameplay.Spawn', included)
        self.assertIn('Road.Main', included)
        self.assertIn('Building.Parcel', included)


if __name__ == '__main__': unittest.main()
