"""SSLM狼的可编辑咬击/侧倒死亡源动作制作工具（Blender后台运行）。

第三层Village内容所有；从已归档CC0 Wolf.blend读取现有骨架和Idle基姿，
不覆盖原稿、不改骨架身份，不写UE二进制资产。输出衍生Blend、两段逐帧烘焙FBX、
预览PNG和制作清单；真实UE导入、AnimBP保存和客户端验证由Monolith后续执行。
例：blender --background --python Tools/Blender/Creatures/AuthorVillageWolfActionSources.py
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Creatures/Wolf"
OUTPUT = SOURCE / "DerivedAnimations"
FPS = 30


def sample_track(frame: int, keys: list[tuple]) -> tuple[float, ...]:
    """逐键使用平滑三次插值，避免冲刺和翻倒出现硬切；返回无单位/源场景单位参数。"""
    for index in range(1, len(keys)):
        before, after = keys[index - 1], keys[index]
        if frame <= after[0]:
            t = max(0.0, (frame - before[0]) / (after[0] - before[0]))
            t = t * t * (3.0 - 2.0 * t)
            return tuple(a + (b - a) * t for a, b in zip(before[1:], after[1:]))
    return tuple(keys[-1][1:])


def rotate_at(pivot: Vector, angle: float, axis: str) -> Matrix:
    """以明确的骨架局部支点旋转；源狼长轴为X，上轴为Z，不改变物体/骨架静止变换。"""
    return Matrix.Translation(pivot) @ Matrix.Rotation(angle, 4, axis) @ Matrix.Translation(-pivot)


def minimum_mesh_height(mesh) -> float:
    """读取真实蒙皮求值后的最低点；只用于源动作接地，不更改UE权威胶囊和导航。"""
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return min((evaluated.matrix_world @ vertex.co).z for vertex in evaluated.data.vertices)


def make_action(rig, mesh, baseline: dict, name: str, last: int, kind: str, floor_z: float):
    """在原骨架逐帧记录姿势；IK仍用原约束求值，FBX导出时烘焙所有骨骼。"""
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    rig.animation_data.action = action
    scene = bpy.context.scene
    scene.frame_start, scene.frame_end = 1, last
    for frame in range(1, last + 1):
        scene.frame_set(frame)
        for bone in rig.pose.bones:
            bone.matrix_basis = baseline[bone.name].copy()
        bpy.context.view_layer.update()
        if kind == "bite":
            # 蓄势压低->前冲伸颈->回收；末帧回到Idle，位移只存在于骨架且禁用UE根运动。
            x, z, pitch, head = sample_track(frame, [
                (1, 0, 0, 0, 0), (6, -0.20, -0.18, -0.10, -0.18),
                (10, 0.46, 0.10, 0.08, 0.32), (13, 0.52, -0.04, 0.05, 0.20),
                (18, 0.18, -0.05, -0.02, -0.04), (25, 0, 0, 0, 0),
            ])
            root = rig.pose.bones["Bone"]
            root.matrix = Matrix.Translation((x, 0, z)) @ rotate_at(root.head.copy(), pitch, "Y") @ root.matrix
            # 蹄端前移量较身体小，保留扑咬的腿部伸展，回收后无累积位移。
            for bone in rig.pose.bones:
                if bone.name.startswith("IK"):
                    front = "Front" in bone.name
                    bone.matrix = Matrix.Translation((x * (0.72 if front else 0.30), 0, max(0.0, z))) @ bone.matrix
            bpy.context.view_layer.update()
            neck = rig.pose.bones["Bone.002"]
            neck.matrix = rotate_at(neck.head.copy(), head, "Y") @ neck.matrix
            muzzle = rig.pose.bones["Bone.003"]
            muzzle.matrix = rotate_at(muzzle.head.copy(), -head * 0.40, "Y") @ muzzle.matrix
        else:
            # 死亡由真实死亡通知触发；身体先失力压低，再绕长轴侧倒，末段保持尸体姿势。
            roll, drop, pitch = sample_track(frame, [
                (1, 0, 0, 0), (7, 0.10, 0.28, -0.12), (15, 0.62, 0.82, -0.06),
                (26, 1.46, 1.52, 0.02), (33, 1.60, 1.58, 0), (43, 1.55, 1.56, 0),
                (49, 1.55, 1.56, 0),
            ])
            pivot = Vector((0.0, 0.0, 1.85))
            collapse = Matrix.Translation((0, 0, -drop)) @ rotate_at(pivot, roll, "X") @ rotate_at(pivot, pitch, "Y")
            for bone in rig.pose.bones:
                if bone.parent is None:
                    bone.matrix = collapse @ bone.matrix
        bpy.context.view_layer.update()
        if kind == "death":
            # 姿势侧倒会改变横截面最低点；按实际蒙皮地面修正全部根和IK，防止尸体穿地。
            correction = (floor_z - minimum_mesh_height(mesh)) / rig.scale.z
            for bone in rig.pose.bones:
                if bone.parent is None:
                    bone.matrix = Matrix.Translation((0, 0, correction)) @ bone.matrix
            bpy.context.view_layer.update()
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path="location", frame=frame, group=bone.name)
            bone.keyframe_insert(data_path="rotation_quaternion", frame=frame, group=bone.name)
            bone.keyframe_insert(data_path="scale", frame=frame, group=bone.name)
    scene.frame_set(1)
    return action


def export_action(rig, mesh, action, last: int):
    """采用原FBX的骨架名与末端骨结构，固定30FPS逐帧烘焙，不导出其他动作。"""
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    rig.select_set(True)
    mesh.select_set(True)
    bpy.context.view_layer.objects.active = rig
    rig.animation_data.action = action
    bpy.context.scene.frame_start, bpy.context.scene.frame_end = 1, last
    path = OUTPUT / f"Wolf_{action.name}.fbx"
    bpy.ops.export_scene.fbx(
        filepath=str(path), use_selection=True, object_types={"ARMATURE", "MESH"},
        axis_forward="-Z", axis_up="Y", apply_unit_scale=True, global_scale=1.0,
        add_leaf_bones=True, bake_anim=True, bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True, bake_anim_step=1.0, bake_anim_simplify_factor=0.0,
    )
    return {"file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "frames": [1, last], "fps": FPS, "duration_seconds": (last - 1) / FPS}


def preview(rig, action, frame: int, label: str):
    """渲染原骨架真实求值姿势，用于源动作视觉复核；不冒充游戏客户端截图。"""
    rig.animation_data.action = action
    scene = bpy.context.scene
    scene.frame_set(frame)
    scene.render.filepath = str(OUTPUT / f"Preview_{label}_{frame:02d}.png")
    bpy.ops.render.render(write_still=True)


def main():
    """读取原稿、增量创建衍生动作和证据；失败由Blender退出异常，原稿不会被覆盖。"""
    OUTPUT.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / "Wolf.blend"))
    rig, mesh = bpy.data.objects["WolfArmature"], bpy.data.objects["Wolf"]
    if bpy.context.object and bpy.context.object.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")
    rig.animation_data.action = bpy.data.actions["Idle"]
    bpy.context.scene.frame_set(0)
    bpy.context.view_layer.update()
    baseline = {bone.name: bone.matrix_basis.copy() for bone in rig.pose.bones}
    floor_z = minimum_mesh_height(mesh)
    scene = bpy.context.scene
    scene.render.fps = FPS
    bite = make_action(rig, mesh, baseline, "Bite", 25, "bite", floor_z)
    death = make_action(rig, mesh, baseline, "Death", 49, "death", floor_z)
    outputs = [export_action(rig, mesh, bite, 25), export_action(rig, mesh, death, 49)]
    scene.frame_start, scene.frame_end = 1, 49
    rig.animation_data.action = death
    scene.frame_set(49)
    # 留存的Blend保留Idle/Walking和新增动作、原网格与原IK约束，可人工继续编辑。
    bpy.ops.wm.save_as_mainfile(filepath=str(OUTPUT / "Wolf_CombatActions.blend"))

    scene.render.engine = "CYCLES"
    scene.cycles.samples = 24
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 640, 400, 100
    scene.world.color = (0.35, 0.35, 0.35)
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.exposure = -0.8
    # 证据采用中性黏土材质观察剪影与接地；保存的源Blend仍保留作者原始材质。
    clay = bpy.data.materials.new("PreviewWolfClay_NotExported")
    clay.use_nodes = True
    shader = next(node for node in clay.node_tree.nodes if node.type == "BSDF_PRINCIPLED")
    shader.inputs["Base Color"].default_value = (0.28, 0.34, 0.36, 1)
    shader.inputs["Roughness"].default_value = 0.72
    mesh.data.materials.clear()
    mesh.data.materials.append(clay)
    bpy.ops.object.camera_add(location=(9, -14, 7))
    camera = bpy.context.object
    target = Vector((0, 0, 1.7))
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.type, camera.data.ortho_scale = "ORTHO", 12
    scene.camera = camera
    bpy.ops.object.light_add(type="AREA", location=(1, -6, 9))
    bpy.context.object.data.energy, bpy.context.object.data.size = 1600, 7
    bpy.ops.mesh.primitive_plane_add(size=200, location=(0, 0, -0.12))
    ground = bpy.context.object
    ground.name = "PreviewGround_NotExported"
    material = bpy.data.materials.new("PreviewGround")
    material.use_nodes = True
    next(node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED").inputs["Base Color"].default_value = (0.14, 0.19, 0.18, 1)
    ground.data.materials.append(material)
    for action, frames in [(bite, [1, 6, 11]), (death, [1, 15, 33, 49])]:
        for frame in frames:
            preview(rig, action, frame, action.name)
    manifest = {"original_source": "../Wolf.blend", "authoring": "SSLM derived from Quaternius CC0 rig",
                "root_motion": False, "outputs": outputs,
                "note_zh": "咬击0.8秒，死亡1.6秒并保持末帧；真实UE导入、骨架兼容与客户端播放尚须Monolith核验。"}
    (OUTPUT / "AnimationSourceManifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    print("WOLF_COMBAT_ACTION_SOURCES=" + json.dumps(manifest, ensure_ascii=False))


if __name__ == "__main__":
    main()
