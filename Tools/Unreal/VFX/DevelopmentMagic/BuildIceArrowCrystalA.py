#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 玄霜棱矢 A 方案晶体源模型及原生导入。

项目内容层专用作者工具：生成连续、有厚度的低面数晶体，不创建碰撞或玩法对象。
源 OBJ 使用厘米、正 X 攻击方向、逐面法线和连续纵向 UV；由 Monolith 调用
import_candidate 后才生成真实 UE StaticMesh。仅写 Review 候选，拒绝覆盖已有资产。
不修改平台通用材质，不把概念图用作三维技能贴图。
"""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow/Review/SM_DBA_IceArrow_CrystalA.obj'
DEST = '/DBAPresentationPack_Core/VFX/IceArrow/Meshes/Review'


def build_source(revision=1):
    """生成闭合主晶体；错位八边环产生宽大切面，避免规则圆锥和分离尾翼。"""
    if SOURCE.exists():
        raise RuntimeError('候选源模型已存在，禁止无审查覆盖')
    rings = [(-59, 16, 13, .10), (-36, 31, 25, .0), (-8, 29, 23, .15),
             (24, 20, 16, -.08), (49, 10, 8, .06)]
    if revision == 2:
        # 二次造型：后端收尖、宽切面集中在中后段；破除圆筒端盖与规则圆锥剪影。
        rings = [(-58, 7, 5, .05), (-37, 29, 22, -.18), (-9, 32, 26, .12),
                 (18, 22, 19, -.15), (48, 11, 8, .08)]
    vertices, uvs, faces = [], [], []
    for j, (x, ry, rz, phase) in enumerate(rings):
        for k in range(8):
            angle = 2 * math.pi * k / 8 + math.pi / 8 + phase
            variation = 1 + .08 * math.sin(k * 2.7 + j * 1.9)
            vertices.append((x + 2.5 * math.sin(k * 2.1 + j), ry * math.cos(angle) * variation, rz * math.sin(angle) * variation))
            uvs.append(((x + 65) / 140, k / 8))
    for j in range(len(rings) - 1):
        for k in range(8):
            a, b = j * 8 + k, j * 8 + (k + 1) % 8
            c, d = (j + 1) * 8 + (k + 1) % 8, (j + 1) * 8 + k
            faces.extend([(a, b, d), (b, c, d)] if (j + k) % 2 else [(a, b, c), (a, c, d)])
    vertices.extend([(-72 if revision == 2 else -62, 0, 0), (76, 0, 0)])
    uvs.extend([(0, .5), (1, .5)])
    for k in range(8):
        faces.append((40, (k + 1) % 8, k))
        faces.append((41, 32 + k, 32 + (k + 1) % 8))
    lines = ['# SSLM 玄霜棱矢 A：正 X 攻击方向；厘米；闭合切面；无碰撞', 'o IceArrowCrystalA', 's off']
    lines.extend('v %.6f %.6f %.6f' % v for v in vertices)
    lines.extend('vt %.6f %.6f' % uv for uv in uvs)
    oriented = []
    for tri in faces:
        points = [vertices[i] for i in tri]
        u = tuple(points[1][k] - points[0][k] for k in range(3))
        v = tuple(points[2][k] - points[0][k] for k in range(3))
        normal = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
        center = tuple(sum(p[k] for p in points) / 3 for k in range(3))
        # 主体绕 X 轴闭合；端盖另以 X 符号确定外向法线。
        outward = normal[1]*center[1]+normal[2]*center[2] if 40 not in tri and 41 not in tri else normal[0]*center[0]
        if outward < 0:
            tri = (tri[0], tri[2], tri[1])
            normal = tuple(-n for n in normal)
        length = math.sqrt(sum(n*n for n in normal))
        if length < 1e-6:
            raise RuntimeError('源模型含退化三角形')
        lines.append('vn %.6f %.6f %.6f' % tuple(n/length for n in normal))
        oriented.append(tri)
    for index, tri in enumerate(oriented, 1):
        lines.append('f ' + ' '.join(f'{v+1}/{v+1}/{index}' for v in tri))
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    SOURCE.write_text('\n'.join(lines)+'\n', encoding='utf-8')
    return {'source': str(SOURCE), 'vertices': len(vertices), 'triangles': len(faces)}


def import_candidate():
    """仅由 Monolith 在所属编辑器执行；导入闭合网格，保留切面法线并禁用碰撞。"""
    import unreal
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    assert unreal.Paths.get_project_file_path().replace('\\', '/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    path = DEST + '/' + SOURCE.stem
    assert SOURCE.exists() and not unreal.EditorAssetLibrary.does_asset_exist(path)
    task = unreal.AssetImportTask()
    task.set_editor_properties({'filename': str(SOURCE), 'destination_path': DEST,
                               'destination_name': SOURCE.stem, 'automated': True,
                               'save': True, 'replace_existing': False})
    options = unreal.FbxImportUI()
    options.set_editor_properties({'import_mesh': True, 'import_as_skeletal': False,
                                  'import_animations': False, 'import_materials': False,
                                  'import_textures': False, 'automated_import_should_detect_type': False,
                                  'mesh_type_to_import': unreal.FBXImportType.FBXIT_STATIC_MESH})
    options.static_mesh_import_data.set_editor_properties({'generate_lightmap_u_vs': False,
                                                          'auto_generate_collision': False,
                                                          'normal_import_method': unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS})
    task.set_editor_property('options', options)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.StaticMesh)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    return {'asset': mesh.get_path_name(), 'bounds': str(mesh.get_bounds())}


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', type=int, choices=[1, 2], default=1)
    args = parser.parse_args()
    if args.revision == 2:
        SOURCE = SOURCE.with_name('SM_DBA_IceArrow_CrystalA2.obj')
    print(build_source(args.revision))
