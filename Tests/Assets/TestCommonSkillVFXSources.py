#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM通用VFX源几何合同测试：20模板、有限可导入几何和独立主体/边缘。

只检查项目制作源，不将静态通过冒充Niagara编译、真实渲染或人工视觉验收。
"""
import importlib.util
import math
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Tools/Unreal/VFX/CommonSkillVFX/BuildCommonSkillVFXSources.py'


class CommonSkillVFXSources(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location('common_vfx_sources', SOURCE)
        cls.api = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.api)

    def test_twenty_distinct_templates_four_per_category(self):
        rows = self.api.catalog()
        self.assertEqual(len(rows), 20)
        self.assertEqual(len({r['key'] for r in rows}), 20)
        for category in ('Attack', 'Defense', 'Control', 'Healing', 'Movement'):
            self.assertEqual(sum(r['key'].startswith(category + '_') for r in rows), 4)

    def test_all_meshes_have_finite_positions_uvs_and_normals(self):
        for row in self.api.catalog():
            parts = self.api.build_parts(row['key'])
            self.assertEqual(set(parts), {'Core', 'Rim', 'Accent'})
            for name, mesh in parts.items():
                with self.subTest(template=row['key'], part=name):
                    self.assertGreater(len(mesh.faces), 0)
                    self.assertLess(len(mesh.faces), 35000)
                    for position, normal, uv in mesh.vertices:
                        self.assertTrue(all(math.isfinite(v) for v in (*position, *normal, *uv)))
                        self.assertAlmostEqual(sum(v*v for v in normal), 1.0, places=3)
                    for face in mesh.faces:
                        a, b, c = [mesh.vertices[i][0] for i in face]
                        ab = [b[i]-a[i] for i in range(3)]
                        ac = [c[i]-a[i] for i in range(3)]
                        cross = (ab[1]*ac[2]-ab[2]*ac[1], ab[2]*ac[0]-ab[0]*ac[2], ab[0]*ac[1]-ab[1]*ac[0])
                        self.assertGreater(sum(x*x for x in cross), 1e-13)

    def test_geometry_is_spatial_and_bounded_for_gameplay_scale(self):
        for row in self.api.catalog():
            points = [v[0] for m in self.api.build_parts(row['key']).values() for v in m.vertices]
            extents = [max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)]
            with self.subTest(template=row['key']):
                self.assertTrue(all(e > 1 for e in extents))
                self.assertTrue(all(abs(x) < 1800 for p in points for x in p))


if __name__ == '__main__':
    unittest.main()
