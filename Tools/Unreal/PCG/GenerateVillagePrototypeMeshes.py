#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Village PCG（新手村程序化生成）六类非发行技术网格来源。

没有Gaea和项目正式美术时，只为EditGraph/Spawner碰撞、导航及门桥接口制作尺寸正确的
三角网格源OBJ+MTL；只有真实UE5.8 Editor可随后导入为StaticMesh.uasset。
所有几何单位都是厘米，Z向上，种子固定；没有Nanite/LODs/PBR贴图或人工美术审核。
禁止冒充3A正式资产、Gaea地貌、已导入.uasset或服务器可信Bake。
--apply仅首次创建完全不存在的技术源目录；--verify只读核对全部哈希和几何合同。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import random
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
ROOT = WORKSPACE / (
    "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
    "SourceArt/PCG/TechnicalPrototype_v1"
)
ROLES = {
    "ForestCanopy": {"domainId": "Forest.Canopy", "usage": "ClientCosmetic", "heightCm": 560.0},
    "RockScatter": {"domainId": "Rock.Scatter", "usage": "ClientCosmetic", "heightCm": 180.0},
    "AgriCrop": {"domainId": "Agri.Crop", "usage": "ClientCosmetic", "heightCm": 170.0},
    "FieldFence": {"domainId": "Encl.FieldFence", "usage": "StaticWorld", "heightCm": 170.0},
    "FarmGate": {"domainId": "Connector.Gate", "usage": "StaticWorld", "heightCm": 300.0},
    "WoodBridge": {"domainId": "Connector.Bridge", "usage": "StaticWorld", "heightCm": 220.0},
}
MATERIALS = {
    "AgedBark": (.235, .175, .122),
    "CanopyGreen": (.145, .27, .133),
    "LeafHighlight": (.205, .315, .152),
    "PeachBlossom": (.62, .425, .46),
    "BlueGrayRock": (.32, .365, .405),
    "RockShadow": (.25, .29, .345),
    "CropStem": (.37, .45, .17),
    "CropGrain": (.58, .455, .195),
    "WoodFrame": (.33, .215, .128),
    "WoodDeck": (.43, .307, .185),
    "DarkFrame": (.19, .15, .105),
}


def vec_add(a, b):
    return tuple(a[i] + b[i] for i in range(3))


def vec_scale(a, scale):
    return tuple(v * scale for v in a)


def vec_sub(a, b):
    return tuple(a[i] - b[i] for i in range(3))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalized(v):
    length = math.sqrt(sum(x * x for x in v))
    if length <= 1e-6:
        raise ValueError("技术网格方向矢量长度接近0")
    return tuple(x / length for x in v)


class Mesh:
    def __init__(self):
        self.points: list[tuple[float, float, float]] = []
        self.faces: list[tuple[str, tuple[int, int, int]]] = []

    def tri(self, a, b, c, material: str):
        if material not in MATERIALS:
            raise ValueError("网格使用未经登记的材质：" + material)
        normal = cross(vec_sub(b, a), vec_sub(c, a))
        if sum(v * v for v in normal) < 1e-8:
            raise ValueError("技术模型出现退化三角形")
        ids = []
        for point in (a, b, c):
            if not all(math.isfinite(v) for v in point):
                raise ValueError("模型顶点出现NaN/Inf")
            self.points.append(tuple(float(value) for value in point))
            ids.append(len(self.points))
        self.faces.append((material, tuple(ids)))

    def quad(self, a, b, c, d, material, double_sided=False):
        self.tri(a, b, c, material)
        self.tri(a, c, d, material)
        if double_sided:
            self.tri(c, b, a, material)
            self.tri(d, c, a, material)

    def box(self, cx, cy, cz, sx, sy, sz, material):
        x0, x1 = cx - sx / 2, cx + sx / 2
        y0, y1 = cy - sy / 2, cy + sy / 2
        z0, z1 = cz - sz / 2, cz + sz / 2
        p = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
             (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        for a, b, c, d in ((0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
                           (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)):
            self.quad(p[a], p[b], p[c], p[d], material)

    def cylinder(self, start, end, radius0, radius1, material, sides=10):
        axis = normalized(vec_sub(end, start))
        reference = (0.0, 0.0, 1.0) if abs(axis[2]) < .88 else (0.0, 1.0, 0.0)
        tangent = normalized(cross(axis, reference))
        bitangent = normalized(cross(axis, tangent))
        lower, upper = [], []
        for i in range(sides):
            angle = (2 * math.pi * i) / sides
            offset = vec_add(vec_scale(tangent, math.cos(angle)),
                             vec_scale(bitangent, math.sin(angle)))
            lower.append(vec_add(start, vec_scale(offset, radius0)))
            upper.append(vec_add(end, vec_scale(offset, radius1)))
        for i in range(sides):
            nxt = (i + 1) % sides
            self.quad(lower[i], lower[nxt], upper[nxt], upper[i], material)
            self.tri(start, lower[nxt], lower[i], material)
            self.tri(end, upper[i], upper[nxt], material)

    def rock_or_crown(self, center, radii, material, *, seed, rings=9, segments=14, noise=.18):
        rng = random.Random(seed)
        rings_data = []
        for ring in range(1, rings):
            latitude = math.pi * ring / rings
            row = []
            for column in range(segments):
                azimuth = 2 * math.pi * column / segments
                rough = 1 + rng.uniform(-noise, noise)
                row.append((center[0] + radii[0] * math.sin(latitude) * math.cos(azimuth) * rough,
                            center[1] + radii[1] * math.sin(latitude) * math.sin(azimuth) * rough,
                            center[2] + radii[2] * math.cos(latitude) * rough))
            rings_data.append(row)
        top = (center[0], center[1], center[2] + radii[2])
        bottom = (center[0], center[1], center[2] - radii[2])
        for s in range(segments):
            nxt = (s + 1) % segments
            self.tri(top, rings_data[0][s], rings_data[0][nxt], material)
            for ring in range(len(rings_data) - 1):
                self.quad(rings_data[ring][s], rings_data[ring + 1][s],
                          rings_data[ring + 1][nxt], rings_data[ring][nxt], material)
            self.tri(rings_data[-1][nxt], rings_data[-1][s], bottom, material)

    def obj_bytes(self, title: str) -> bytes:
        if not self.faces or len(self.points) != len(self.faces) * 3:
            raise ValueError("技术网格没有可用的独立三角面")
        lines = ["# Village PCG technical prototype; centimeters; Z up; not production AAA asset",
                 "mtllib VillagePCGPrototypeMaterials.mtl", "o " + title]
        lines.extend("v {:.5f} {:.5f} {:.5f}".format(*p) for p in self.points)
        normals = []
        for _, (a, b, c) in self.faces:
            normals.append(normalized(cross(
                vec_sub(self.points[b - 1], self.points[a - 1]),
                vec_sub(self.points[c - 1], self.points[a - 1]))))
        lines.extend("vn {:.8f} {:.8f} {:.8f}".format(*v) for v in normals)
        previous = None
        for i, (material, (a, b, c)) in enumerate(self.faces, 1):
            if material != previous:
                lines.append("usemtl " + material)
                previous = material
            lines.append(f"f {a}//{i} {b}//{i} {c}//{i}")
        return ("\n".join(lines) + "\n").encode("utf-8")


def tree():
    b = Mesh()
    b.cylinder((0, 0, 0), (0, 0, 370), 32, 17, "AgedBark", 14)
    b.cylinder((0, 0, 370), (0, 0, 485), 17, 4, "AgedBark")
    for index in range(8):
        angle = 2 * math.pi * index / 8
        start = (0, 0, 205 + (index % 3) * 47)
        end = (math.cos(angle) * (145 + index % 3 * 17),
               math.sin(angle) * (145 + index % 2 * 13),
               330 + (index % 3) * 50)
        b.cylinder(start, end, 12, 3.5, "AgedBark", 8)
        b.rock_or_crown(end, (105, 95, 80),
                        "CanopyGreen" if index % 3 else "LeafHighlight",
                        seed=200 + index, rings=7, segments=13, noise=.10)
        for j in range(3):
            t = (2 * math.pi * j / 3) + angle
            c = (end[0] + math.cos(t) * 65,
                 end[1] + math.sin(t) * 55, end[2] + 28 + j * 10)
            b.rock_or_crown(c, (15, 13, 10), "PeachBlossom",
                            seed=600 + index * 3 + j, rings=5, segments=7, noise=.09)
    return b


def stone():
    b = Mesh()
    b.rock_or_crown((0, 0, 110), (138, 115, 85), "BlueGrayRock",
                    seed=1312, rings=10, segments=19, noise=.18)
    b.rock_or_crown((105, -28, 85), (77, 66, 62), "RockShadow",
                    seed=21, rings=8, segments=12, noise=.12)
    return b


def crop():
    b = Mesh()
    for n in range(11):
        a = 2 * math.pi * n / 11
        distance = 18 + n % 3 * 8
        x, y = math.cos(a) * distance, math.sin(a) * distance
        height = 105 + n % 4 * 12
        b.cylinder((x, y, 4), (x + 7, y + 5, height), 2.8, 1.0, "CropStem", 6)
        b.rock_or_crown((x + 7, y + 5, height + 9), (8, 7, 26),
                        "CropGrain", seed=47 + n, rings=5, segments=7, noise=.1)
        for side in (-1, 1):
            base = (x, y, 24 + (n % 3) * 8)
            middle = (x + side * 24, y + 9, 45 + n % 6 * 5)
            tip = (x + side * 52, y + 17, 25)
            b.quad(base, middle, tip, (x + side * 12, y + 17, 41),
                   "CropStem", double_sided=True)
    return b


def fence():
    b = Mesh()
    for x in (-145, 0, 145):
        b.box(x, 0, 78, 16, 17, 156, "WoodFrame")
        b.box(x, 0, 158, 23, 23, 11, "DarkFrame")
    for z in (56, 121):
        b.box(0, 1, z, 300, 11, 13, "WoodDeck")
    return b


def gate():
    b = Mesh()
    for x in (-173, 173):
        b.box(x, 0, 133, 25, 28, 266, "WoodFrame")
        b.box(x, 0, 272, 35, 36, 16, "DarkFrame")
    b.box(0, 0, 247, 375, 25, 19, "WoodFrame")
    b.box(0, 0, 288, 415, 50, 12, "DarkFrame")
    for x in (-150, 150):
        b.box(x, 0, 40, 38, 40, 80, "RockShadow")
    return b


def bridge():
    b = Mesh()
    for index in range(32):
        x = -387.5 + index * 25
        h = 84 + 52 * (1 - (x / 425) ** 2)
        b.box(x, 0, h, 23, 255, 15, "WoodDeck")
    for y in (-117, 117):
        for x in (-385, -242, -99, 44, 187, 330, 385):
            h = 84 + 52 * (1 - (x / 425) ** 2)
            b.box(x, y, h + 43, 13, 16, 94, "WoodFrame")
        # 分段栏杆保留真实可测试长度和连接口，不能当成已验证的NavMesh。
        for n in range(31):
            x = -385 + n * 25
            h = 84 + 52 * (1 - (x / 425) ** 2)
            b.box(x, y, h + 88, 27, 13, 14, "DarkFrame")
    for x in (-360, 360):
        b.box(x, 0, 43, 35, 220, 86, "WoodFrame")
    return b


BUILDERS = {
    "ForestCanopy": tree,
    "RockScatter": stone,
    "AgriCrop": crop,
    "FieldFence": fence,
    "FarmGate": gate,
    "WoodBridge": bridge,
}


def materials_file() -> bytes:
    lines = ["# RGB values are technical matte prototypes, not final PBR textures"]
    for name, color in MATERIALS.items():
        lines.extend((f"newmtl {name}", "Kd {:.5f} {:.5f} {:.5f}".format(*color),
                      "Ka 0.00000 0.00000 0.00000", "Ks 0.01200 0.01200 0.01200",
                      "Ns 12.0", "d 1.0", "illum 2"))
    return ("\n".join(lines) + "\n").encode("utf-8")


def make_sources() -> tuple[dict[str, bytes], dict]:
    outputs: dict[str, bytes] = {}
    rows = []
    for role, generator in BUILDERS.items():
        mesh = generator()
        data = mesh.obj_bytes("TP_DBA_PCG_" + role)
        positions = mesh.points
        extents = [
            [round(min(point[axis] for point in positions), 3),
             round(max(point[axis] for point in positions), 3)]
            for axis in range(3)
        ]
        if extents[2][0] < -0.01 or len(mesh.faces) < 20:
            raise ValueError("技术网格轴向或三角面异常：" + role)
        name = "TP_DBA_PCG_" + role + ".obj"
        outputs[name] = data
        rows.append({"role": role, "domainId": ROLES[role]["domainId"],
                     "usage": ROLES[role]["usage"],
                     "relativePath": name, "triangles": len(mesh.faces),
                     "boundsCm": extents, "sha256": hashlib.sha256(data).hexdigest()})
    mtl = materials_file()
    outputs["VillagePCGPrototypeMaterials.mtl"] = mtl
    meta = {
        "schemaVersion": 1,
        "sourceKind": "procedural_technical_prototype_not_approved",
        "engineTarget": "UE5.8",
        "units": "centimeters_Z_up",
        "outputUsage": "Editor technical samples only; not final artistic assets or Server authority",
        "galeaRequired": False,
        "meshSources": rows,
        "material": {"relativePath": "VillagePCGPrototypeMaterials.mtl",
                     "sha256": hashlib.sha256(mtl).hexdigest()},
        "humanArtApproval": "pending",
        "unrealAssetCreated": False,
    }
    outputs["VillagePCGTechnicalSourceManifest_v1.json"] = (
        json.dumps(meta, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    return outputs, meta


def main() -> int:
    parser = argparse.ArgumentParser(description="六类PCG非发行技术几何源创建或只读审核")
    modes = parser.add_mutually_exclusive_group(required=True)
    modes.add_argument("--apply", action="store_true", help="首次创建完整隔离源目录")
    modes.add_argument("--verify", action="store_true", help="只读按SHA-256和三角面检查")
    flags = parser.parse_args()
    files, report = make_sources()
    if flags.apply:
        if ROOT.exists():
            raise FileExistsError("原型源目录已存在，拒绝覆盖：" + str(ROOT))
        ROOT.mkdir(parents=True)
        for filename, data in files.items():
            (ROOT / filename).write_bytes(data)
        print("VILLAGE_PCG_PROTOTYPE_SOURCE_WRITTEN", len(files), ROOT)
    else:
        if not ROOT.is_dir() or {p.name for p in ROOT.iterdir()} != set(files):
            raise ValueError("六类技术源文件集合缺失或出现未知文件")
        for name, data in files.items():
            if (ROOT / name).read_bytes() != data:
                raise ValueError("源网格与固定生成参数SHA不一致：" + name)
    print("VILLAGE_PCG_TECHNICAL_MESH_SOURCE_SHA_PASS", len(report["meshSources"]))
    for item in report["meshSources"]:
        print("TECHNICAL_MESH", item["role"], "triangles", item["triangles"],
              "sha256", item["sha256"])
    print("TECHNICAL_ONLY_NOT_GAEA_NOT_FINAL_ART_NOT_IMPORTED_INTO_UE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
