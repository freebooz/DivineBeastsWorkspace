# -*- coding: utf-8 -*-
"""寒冰箭A3三维源网格的独立回归：闭合拓扑、UV、面法线及正X前向，不启动虚幻引擎。

运行：python Tests/Assets/TestIceArrowCrystalA3Geometry.py
仅验证OBJ源文件几何结构；不声称UE StaticMesh导入、Niagara编译、GPU实测或视觉验收。
"""
from collections import defaultdict
from math import isfinite, sqrt
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
OBJ = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow/Review/SM_DBA_IceArrow_CrystalA3Validated.obj"


def parse_source(path=OBJ):
    """解析本项目OBJ的v/vt/vn面引用；不修改候选资产。"""
    vertices, texcoords, normals, faces = [], [], [], []
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if not parts or parts[0].startswith("#"):
            continue
        if parts[0] == "v":
            vertices.append(tuple(float(x) for x in parts[1:]))
        elif parts[0] == "vt":
            texcoords.append(tuple(float(x) for x in parts[1:]))
        elif parts[0] == "vn":
            normals.append(tuple(float(x) for x in parts[1:]))
        elif parts[0] == "f":
            faces.append([tuple(int(i) - 1 for i in ref.split("/")) for ref in parts[1:]])
    return vertices, texcoords, normals, faces


class IceArrowA3GeometryTests(unittest.TestCase):
    """验证候选可作为真实网格导入的结构前提，而非只检查文件存在。"""

    @classmethod
    def setUpClass(cls):
        cls.vertices, cls.uvs, cls.normals, cls.faces = parse_source()

    def test_geometry_and_forward(self):
        """长轴为X且前端尖点朝正X，模型预算保持在低面数范围。"""
        self.assertEqual(len(self.vertices), 122)
        self.assertEqual(len(self.faces), 228)
        self.assertTrue(all(len(p) == 3 and all(isfinite(x) for x in p) for p in self.vertices))
        xs = [v[0] for v in self.vertices]
        self.assertAlmostEqual(max(xs), 78.0, places=4)
        self.assertAlmostEqual(min(xs), -74.0, places=4)
        self.assertTrue(any(abs(x-78) < 0.001 and abs(y) < 0.001 and abs(z) < 0.001
                            for x, y, z in self.vertices))
        self.assertLessEqual(max(xs)-min(xs), 160.0)

    def test_uv_and_face_normals(self):
        """UV全部在0—1，法线单位化且与三角面朝向一致。"""
        self.assertTrue(self.uvs)
        self.assertTrue(self.normals)
        self.assertTrue(all(len(t) == 2 and all(isfinite(x) and 0 <= x <= 1 for x in t)
                            for t in self.uvs))
        for face in self.faces:
            self.assertEqual(len(face), 3)
            points = []
            for vi, ti, ni in face:
                self.assertGreaterEqual(vi, 0)
                self.assertLess(vi, len(self.vertices))
                self.assertGreaterEqual(ti, 0)
                self.assertLess(ti, len(self.uvs))
                self.assertGreaterEqual(ni, 0)
                self.assertLess(ni, len(self.normals))
                points.append(self.vertices[vi])
            p, q, r = points
            a = [q[j] - p[j] for j in range(3)]
            b = [r[j] - p[j] for j in range(3)]
            cross = (a[1]*b[2]-a[2]*b[1],
                     a[2]*b[0]-a[0]*b[2],
                     a[0]*b[1]-a[1]*b[0])
            size = sqrt(sum(v*v for v in cross))
            self.assertGreater(size, 0.01)
            normal = self.normals[face[0][2]]
            self.assertAlmostEqual(sqrt(sum(v*v for v in normal)), 1.0, delta=0.0001)
            self.assertGreater(sum(cross[j]*normal[j] for j in range(3))/size, 0.9999)

    def test_closed_and_consistently_wound(self):
        """每条无向边恰有两个面、方向相反，排除开放裂口和内部反转面。"""
        edges = defaultdict(list)
        for face in self.faces:
            indices = [x[0] for x in face]
            for i in range(3):
                a, b = indices[i], indices[(i + 1) % 3]
                key = tuple(sorted((a, b)))
                edges[key].append((a, b))
        self.assertTrue(edges)
        for edge, directions in edges.items():
            self.assertEqual(len(directions), 2, f"非闭合边：{edge}")
            self.assertEqual(directions[0], tuple(reversed(directions[1])), f"同向边：{edge}")


if __name__ == "__main__":
    unittest.main()
