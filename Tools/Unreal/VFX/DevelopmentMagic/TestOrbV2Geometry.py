# -*- coding: utf-8 -*-
"""SSLM V2返工源网格回归：冰晶必须是闭合不规则球状晶体，不得退化为方盒。
该检查仅证明网格几何质量，不代表UE材质、Niagara或人工视觉通过。
"""
import importlib.util, unittest, math
from collections import Counter
from pathlib import Path
class IceGeometryContract(unittest.TestCase):
    def test_source_module_exists(self):
        self.assertIsNotNone(importlib.util.find_spec('OrbV2Geometry'),'V2冰晶几何作者尚未实现')
    def test_ice_is_closed_irregular_crystal(self):
        spec=importlib.util.find_spec('OrbV2Geometry')
        self.assertIsNotNone(spec,'缺少返工几何作者，拒绝跳过')
        import OrbV2Geometry as g
        verts,faces=g.make_ice_crystal()
        self.assertGreaterEqual(len(verts),42)
        self.assertGreaterEqual(len(faces),80)
        edges=Counter(tuple(sorted((f[i],f[(i+1)%3]))) for f in faces for i in range(3))
        self.assertTrue(all(n==2 for n in edges.values()),'晶体必须封闭，不能有裂面')
        self.assertTrue(all(all(math.isfinite(v) for v in p) for p in verts))
        radii=[math.sqrt(sum(v*v for v in p)) for p in verts]
        self.assertGreater(max(radii)-min(radii),5,'必须有自然不规则轮廓')
        for axis in range(3):
            span=max(v[axis] for v in verts)-min(v[axis] for v in verts)
            self.assertGreater(span,40);self.assertLess(span,90)
    def test_sphere_preserves_round_core(self):
        spec=importlib.util.find_spec('OrbV2Geometry')
        self.assertIsNotNone(spec,'缺少火球核心几何作者')
        import OrbV2Geometry as g
        verts,faces=g.make_icosphere(3,28.0)
        self.assertGreater(len(verts),200)
        self.assertTrue(all(abs(math.sqrt(sum(x*x for x in v))-28)<1e-5 for v in verts))
if __name__=='__main__':unittest.main(verbosity=2)
