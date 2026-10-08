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

skeleton = unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SK_Mannequin_Skeleton')
bone_names = [str(name) for name in unreal.AnimPoseExtensions.get_bone_names(skeleton.get_reference_pose())] if skeleton else []
for required_bone in ('root', 'pelvis', 'head'):
    if required_bone not in bone_names:
        errors.append('公共骨架缺少必要骨骼: ' + required_bone)

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

print(json.dumps({'profilesChecked': profiles_checked, 'boneCount': len(bone_names), 'materialsChecked': materials_checked, 'errors': errors}, ensure_ascii=False))
assert not errors, '角色预览资产回归失败，见上方具体缺陷。'
