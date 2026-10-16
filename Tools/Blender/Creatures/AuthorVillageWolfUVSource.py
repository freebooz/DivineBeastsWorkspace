"""为SSLM已归档CC0狼制作独立UV0源，保留原骨架、拓扑、蒙皮和变换。

只生成Wolf_Textured.blend/FBX和审计JSON，不操作UE资产。保存源与原Blend逐项比较，
再独立重导入原/新FBX验证蒙皮和骨架；失败抛异常，禁止把失败结果作为重导入依据。
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Creatures/Wolf"
BLEND = SOURCE / "Wolf_Textured.blend"
FBX = SOURCE / "Wolf_Textured.fbx"


def raw_signature(mesh) -> dict:
    """逐索引记录源网格和权重；UV只新增Loop数据，不能改变这些值或Actor原点。"""
    return {"vertices": [list(v.co) for v in mesh.data.vertices],
            "faces": [list(p.vertices) for p in mesh.data.polygons],
            "weights": [[(mesh.vertex_groups[g.group].name, g.weight) for g in v.groups] for v in mesh.data.vertices],
            "matrix_world": [v for row in mesh.matrix_world for v in row]}


def export_signature(path: Path) -> dict:
    """独立空场景导入FBX，比较世界坐标+骨骼权重与父链，允许文件内部索引重排。"""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path), use_anim=False)
    mesh = bpy.data.objects["Wolf"]
    rig = bpy.data.objects["WolfArmature"]
    vertices = []
    for vertex in mesh.data.vertices:
        point = mesh.matrix_world @ vertex.co
        weights = tuple(sorted((mesh.vertex_groups[g.group].name, round(g.weight, 5)) for g in vertex.groups))
        vertices.append((tuple(round(v, 4) for v in point), weights))
    faces = sorted(tuple(sorted(vertices[i][0] for i in polygon.vertices)) for polygon in mesh.data.polygons)
    return {"vertices": sorted(vertices), "faces": faces,
            "bone_parents": {b.name: b.parent.name if b.parent else None for b in rig.data.bones},
            "bone_matrices": {b.name: [v for row in b.matrix_local for v in row] for b in rig.data.bones},
            "uv": [(layer.name, len(layer.data)) for layer in mesh.data.uv_layers]}


def validate_uv(mesh) -> dict:
    """核对UV0覆盖全部角点、位于0..1且每个面面积非零；源文件保留独立可编辑岛。"""
    layers = mesh.data.uv_layers
    assert len(layers) == 1 and layers[0].name == "UV0"
    layer = layers[0]
    assert len(layer.data) == len(mesh.data.loops)
    coordinates = [loop.uv.copy() for loop in layer.data]
    assert all(math.isfinite(v) and -1e-6 <= v <= 1.000001 for uv in coordinates for v in uv)
    minimum_area = 1.0
    total_area = 0.0
    for face in mesh.data.polygons:
        points = [coordinates[i] for i in face.loop_indices]
        area = abs(sum(points[i].x * points[(i + 1) % len(points)].y - points[(i + 1) % len(points)].x * points[i].y
                       for i in range(len(points))) / 2)
        assert area > 1e-10, f"UV面不能退化: {face.index}"
        minimum_area, total_area = min(minimum_area, area), total_area + area
    return {"channel": "UV0", "loop_count": len(coordinates), "minimum_face_area": minimum_area,
            "total_face_area": total_area, "bounds": [min(uv[i] for uv in coordinates) for i in range(2)] +
            [max(uv[i] for uv in coordinates) for i in range(2)]}


def main():
    """从只读原稿新增UV0，保存独立源，再完整重开/重导入核验；不覆盖原文件。"""
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / "Wolf.blend"))
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    mesh, rig = bpy.data.objects["Wolf"], bpy.data.objects["WolfArmature"]
    baseline = raw_signature(mesh)
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = mesh
    assert len(mesh.data.uv_layers) == 0, "原稿若已有UV必须先重新评估，不能覆盖"
    mesh.data.uv_layers.new(name="UV0")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    # 低模狼以66度硬转折分岛并留足像素边距；仅改UV，不合并顶点或修改法线/蒙皮。
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.025, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    assert raw_signature(mesh) == baseline, "UV制作不得改变原顶点、拓扑、权重和世界矩阵"
    uv = validate_uv(mesh)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.export_scene.fbx(filepath=str(FBX), use_selection=True, object_types={"ARMATURE", "MESH"},
        axis_forward="-Z", axis_up="Y", apply_unit_scale=True, global_scale=1.0,
        add_leaf_bones=True, bake_anim=False)

    # 独立重开证明保存的Blend有同一几何及可编辑UV，不用内存态当交付证据。
    bpy.ops.wm.open_mainfile(filepath=str(BLEND))
    assert raw_signature(bpy.data.objects["Wolf"]) == baseline
    validate_uv(bpy.data.objects["Wolf"])
    original, textured = export_signature(SOURCE / "Wolf.fbx"), export_signature(FBX)
    assert original["vertices"] == textured["vertices"], "重导入FBX顶点位置/骨骼权重有差异"
    assert original["faces"] == textured["faces"], "重导入FBX面拓扑有差异"
    assert original["bone_parents"] == textured["bone_parents"], "骨骼父链有差异"
    maximum_error = max(abs(a - b) for name, matrix in textured["bone_matrices"].items()
                        for a, b in zip(matrix, original["bone_matrices"][name]))
    assert maximum_error < 0.0001
    assert len(textured["uv"]) == 1 and textured["uv"][0][1] == uv["loop_count"]
    result = {"source": "Wolf.blend", "outputs": [BLEND.name, FBX.name], "uv": uv,
              "source_indexed_vertices_faces_weights_world_matrix_unchanged": True,
              "reimport_fbx_vertex_weights_and_geometric_faces_match": True,
              "vertex_count": len(textured["vertices"]), "face_count": len(textured["faces"]),
              "bone_count": len(textured["bone_parents"]), "max_bone_rest_matrix_error": maximum_error,
              "fbx_sha256": hashlib.sha256(FBX.read_bytes()).hexdigest(),
              "ue_reimport": "pending; existing Skeleton, ImportAnimations=false, no new PhysicsAsset, preserve materials"}
    (SOURCE / "Wolf_Textured_Validation.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print("WOLF_UV_SOURCE_VERIFIED=" + json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
