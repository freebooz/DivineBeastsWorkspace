# -*- coding: utf-8 -*-
"""角色预览资产回归：由锁定 UE 编辑器执行，只读检查十二外观与共享材质。

使用本项目视觉清单遍历真实保存资产，拒绝俯仰误写、骨架断链、骨骼用途缺失
和空纹理采样。失败意味着不能把 Cook 的零退出码视为角色表现交付。
不访问账号、网络、玩法状态，也不修改或保存任何资产。
"""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[2]
manifest = json.loads((root / 'Tools/Unreal/Characters/ZodiacPlaceholderCharacters.json').read_text(encoding='utf-8-sig'))
errors = []
profiles_checked = 0
meshes_checked = set()
loaded_meshes = []
for hero in manifest['heroes']:
    suffix = hero['heroId'].rsplit('.', 1)[1]
    path = '/' + hero['pack'] + '/Characters/DA_Appearance_Zodiac_' + suffix
    profile = unreal.load_asset(path)
    if not profile:
        errors.append(path + ': 外观资产缺失')
        continue
    profiles_checked += 1
    rotation = profile.get_editor_property('mesh_relative_rotation')
    if abs(rotation.pitch) > 0.01 or abs(rotation.roll) > 0.01 or abs(rotation.yaw + 90.0) > 0.01:
        errors.append(path + ': 应为Yaw=-90，实际 ' + str(rotation))
    # Python反射会把可解析软引用转换为已加载UObject，不能把对象repr当资产路径。
    mesh = profile.get_editor_property('skeletal_mesh')
    if not isinstance(mesh, unreal.SkeletalMesh):
        mesh = unreal.load_asset(str(mesh))
    if not mesh or not mesh.get_editor_property('skeleton'):
        errors.append(path + ': 骨骼网格或骨架缺失')
    elif mesh.get_path_name() not in meshes_checked:
        meshes_checked.add(mesh.get_path_name())
        loaded_meshes.append(mesh)
        # 同名兼容身份或网格指向该Skeleton并不能证明真实骨树兼容。
        # 从网格自身遍历89根原始骨骼，旧UE4骨架缺21根时必须在Cook前失败。
        mesh_bones = ['root']
        for bone in mesh_bones:
            mesh_bones.extend(str(child) for child in mesh.get_bone_children(bone))
        mesh_skeleton = mesh.get_editor_property('skeleton')
        skeleton_names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(mesh_skeleton.get_reference_pose())}
        missing = sorted(set(mesh_bones) - skeleton_names)
        if missing:
            errors.append(mesh.get_path_name() + ': 骨架缺少网格骨骼 ' + ','.join(missing))
    if mesh and not mesh.get_editor_property('physics_asset'):
        errors.append(path + ': 既定物理资产引用缺失')
    # 十二原型共用一套母材质，通过每英雄实例区分颜色；实例存在但父链断开也必须失败。
    overrides = list(profile.get_editor_property('material_overrides'))
    if len(overrides) != 1 or not overrides[0]:
        errors.append(path + ': 原型颜色材质缺失')
    else:
        material_instance = overrides[0]
        parent = material_instance.get_editor_property('parent')
        if not parent or parent.get_path_name().split('.')[0] != '/DBAContentPack_Common/Materials/M_DBA_PrototypeCharacterColor':
            errors.append(path + ': 原型材质母链不匹配')

master = unreal.load_asset('/DBAContentPack_Common/Materials/M_DBA_PrototypeCharacterColor')
if not master or not master.get_editor_property('used_with_skeletal_mesh'):
    errors.append('生肖公共母材质缺少SkeletalMesh用途')

skeleton = unreal.load_asset('/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin')
bone_names = [str(name) for name in unreal.AnimPoseExtensions.get_bone_names(skeleton.get_reference_pose())] if skeleton else []
for required_bone in ('root', 'pelvis', 'spine_04', 'spine_05', 'neck_02', 'head'):
    if required_bone not in bone_names:
        errors.append('公共骨架缺少必要骨骼: ' + required_bone)
dependencies = unreal.AssetRegistryHelpers.get_asset_registry().get_dependencies(
    '/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin',
    unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True))
if any(str(path).startswith('/Game/Characters/') for path in dependencies):
    errors.append('公共骨架保留不存在的模板工程路径，须通过引擎软引用迁移修复')

sequence = unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/AS_DBA_PreviewIdle')
blueprint = unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_PreviewIdle')
poses_checked = 0
if not sequence or sequence.get_skeleton() != skeleton or not blueprint or blueprint.get_editor_property('target_skeleton') != skeleton:
    errors.append('Idle序列/动画蓝图必须使用完整UE5骨架')
else:
    model = sequence.controller.get_model_interface()
    # UE的FName不区分大小写；Sequencer轨道保存后的名称大小写可与参考骨树不同。
    tracks = {str(name).casefold() for name in model.get_bone_track_names()}
    if not {name.casefold() for name in bone_names}.issubset(tracks):
        errors.append('Idle未按完整UE5参考姿势重建骨骼轨道')
    if model.get_number_of_frames() != 120 or model.get_frame_rate().numerator != 30 or model.get_frame_rate().denominator != 1:
        errors.append('开发Idle必须保持四秒/30fps循环')
    for mesh in loaded_meshes:
        # 实际在男女模型的BoneContainer中采样动画，检查骨长与缩放；不只校验资源标签。
        # 呼吸用旋转完成，身体平移必须保留各网格参考比例，防止Manny骨长拉长Quinn。
        options = unreal.AnimPoseEvaluationOptions()
        options.set_editor_property('optional_skeletal_mesh', mesh)
        options.set_editor_property('should_retarget', True)
        first_pose = None
        for seconds in (0.0, 1.0, 2.0, 3.0, 4.0):
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, seconds, options)
            poses_checked += 1
            if not pose.is_valid():
                errors.append(mesh.get_name() + ': Idle姿势无法真实采样，时间=' + str(seconds))
                continue
            if first_pose is None:
                first_pose = pose
            if seconds == 1.0:
                initial_chest = unreal.AnimPoseExtensions.get_bone_pose(first_pose, 'spine_05', unreal.AnimPoseSpaces.LOCAL).rotation
                current_chest = unreal.AnimPoseExtensions.get_bone_pose(pose, 'spine_05', unreal.AnimPoseSpaces.LOCAL).rotation
                if max(abs(getattr(initial_chest, component) - getattr(current_chest, component)) for component in ('x', 'y', 'z', 'w')) < 0.0001:
                    errors.append(mesh.get_name() + ': Idle胸部姿势未随时间变化')
            for bone in ('pelvis', 'spine_03', 'spine_04', 'spine_05', 'neck_01', 'neck_02', 'head', 'upperarm_l', 'thigh_l', 'foot_l'):
                transform = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                reference = unreal.AnimPoseExtensions.get_ref_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)
                delta = transform.translation - reference.translation
                if delta.length() > 0.1:
                    errors.append(mesh.get_name() + ':' + bone + ': Idle改变骨长/位置，时间=' + str(seconds))
                if max(abs(transform.scale3d.x - 1.0), abs(transform.scale3d.y - 1.0), abs(transform.scale3d.z - 1.0)) > 0.001:
                    errors.append(mesh.get_name() + ':' + bone + ': Idle存在骨骼缩放')
                if seconds == 0.0:
                    reference_dot = sum(getattr(reference.rotation, component) * getattr(transform.rotation, component) for component in ('x', 'y', 'z', 'w'))
                    if abs(abs(reference_dot) - 1.0) > 0.00001:
                        errors.append(mesh.get_name() + ':' + bone + ': Idle起始姿势扭曲参考关节方向')
                if seconds == 4.0:
                    initial = unreal.AnimPoseExtensions.get_bone_pose(first_pose, bone, unreal.AnimPoseSpaces.LOCAL)
                    dot = sum(getattr(initial.rotation, component) * getattr(transform.rotation, component) for component in ('x', 'y', 'z', 'w'))
                    if abs(abs(dot) - 1.0) > 0.00001:
                        errors.append(mesh.get_name() + ':' + bone + ': Idle循环接缝不连续')

materials_checked = 0
for branch in ('DBA', 'Standard'):
    path = '/DBAContentPack_Common/Mannequins/' + branch + '/Materials/M_Mannequin'
    material = unreal.load_asset(path)
    if not material:
        errors.append(path + ': 基础材质缺失')
        continue
    materials_checked += 1
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        if isinstance(expression, unreal.MaterialExpressionTextureSample) and not expression.get_editor_property('texture'):
            errors.append(path + ':' + expression.get_name() + ': 纹理为空')

print(json.dumps({'profilesChecked': profiles_checked, 'meshesChecked': len(meshes_checked), 'posesChecked': poses_checked, 'boneCount': len(bone_names), 'materialsChecked': materials_checked, 'errors': errors}, ensure_ascii=False))
assert not errors, '角色预览资产回归失败，见上方具体缺陷。'
