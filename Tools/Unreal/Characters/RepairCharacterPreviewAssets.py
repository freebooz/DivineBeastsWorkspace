# -*- coding: utf-8 -*-
"""开发角色预览资源修复，由锁定UE编辑器/Monolith执行。
验证UE5骨架绑定，恢复公共物理资产/材质并按正确参考姿势重建开发待机动画；
不改角色权威定义、不生成UI、不删除旧UE4骨架或网格。
执行前须备份相关资产并显式启用GamePlatformDeveloperTools编辑器插件；失败保留已保存项，通过备份可逐文件回退。
"""
import json
import math
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[3]
COMMON = '/DBAContentPack_Common/Mannequins'
changed = []

def load_required(path):
    """已交付依赖必须真实存在，不能用空资源或默认成功掩盖缺失。"""
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError('缺少既定角色资源: ' + path)
    return asset

def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('角色资产保存失败: ' + asset.get_path_name())
    changed.append(asset.get_path_name())

skeleton = load_required(COMMON + '/UE5/Meshes/SK_Mannequin')
physics = load_required(COMMON + '/DBA/Meshes/SK_Mannequin_PhysicsAsset')
# 旧SK_Mannequin_Skeleton属于UE4，不能因为同名Mannequin就用于UE5网格。
# 写入任何资产之前检查真实骨骼覆盖；绑定由Monolith属性动作显式修改，失败停止。
reference_pose = skeleton.get_reference_pose()
skeleton_names = {str(name) for name in unreal.AnimPoseExtensions.get_bone_names(reference_pose)}
for name in ('Manny', 'Quinn'):
    mesh = load_required(COMMON + '/DBA/Meshes/SKM_' + name + '_Simple')
    mesh_bones = ['root']
    for bone in mesh_bones:
        mesh_bones.extend(str(child) for child in mesh.get_bone_children(bone))
    missing = sorted(set(mesh_bones) - skeleton_names)
    if missing or mesh.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('必须先通过Monolith绑定完整UE5骨架: ' + mesh.get_path_name() + '; 缺少骨骼=' + ','.join(missing))
# 模板的自引用与编辑器重定向来源通过引擎序列化迁移，不手改骨架内部二进制。
# 只扫描本公共骨架包，并映射到本项目既有网格；不改其他内容包或玩法资产。
redirects = unreal.Map(unreal.SoftObjectPath, unreal.SoftObjectPath)
for name in ('SKM_Manny_Simple', 'SKM_Quinn_Simple', 'SK_Mannequin'):
    source = unreal.SoftObjectPath('/Game/Characters/Mannequins/Meshes/' + name + '.' + name)
    branch = '/UE5/Meshes/' if name == 'SK_Mannequin' else '/DBA/Meshes/'
    target = unreal.SoftObjectPath(COMMON + branch + name + '.' + name)
    redirects[source] = target
unreal.AssetToolsHelpers.get_asset_tools().rename_referencing_soft_object_paths([skeleton.get_outermost()], redirects)
standard = load_required(COMMON + '/Standard/Materials/M_Mannequin')
dba = load_required(COMMON + '/DBA/Materials/M_Mannequin')
textures = {
    'Base Texture': '/Manny/T_Manny_01_D',
    'BNormal': '/Manny/T_Manny_01_BN',
    'MRA': '/Manny/T_Manny_01_MRA',
}
for material in (standard, dba):
    for expression in unreal.MaterialEditingLibrary.get_material_expressions(material):
        if isinstance(expression, unreal.MaterialExpressionTextureSample) and not expression.get_editor_property('texture'):
            if isinstance(expression, unreal.MaterialExpressionTextureSampleParameter):
                parameter = str(expression.get_editor_property('parameter_name'))
                relative = textures.get(parameter)
                if not relative:
                    raise RuntimeError('未知空纹理参数，拒绝任意替换: ' + parameter)
            else:
                # 该既定Mannequin节点读取共享UE徽标，不属于角色身份或玩法纹理。
                if expression.get_name() != 'MaterialExpressionTextureSample_0':
                    raise RuntimeError('未知空纹理采样: ' + expression.get_name())
                relative = '/Shared/T_UE_Logo_M'
            expression.set_editor_property('texture', load_required(COMMON + '/Standard/Textures' + relative))
    material.set_editor_property('used_with_skeletal_mesh', True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    save(material)

# 已迁移MI的失效父引用按原有材质层级恢复，保留所有现存参数覆盖。
for path, parent in (
    ('/DBA/Materials/Instances/Manny/MI_Manny_01', dba),
    ('/DBA/Materials/Instances/Manny/MI_Manny_02', dba),
    ('/DBA/Materials/Quinn/MI_Quinn_02', load_required(COMMON + '/DBA/Materials/Quinn/MI_Quinn_01')),
):
    material = load_required(COMMON + path)
    if not material.get_editor_property('parent'):
        unreal.MaterialEditingLibrary.set_material_instance_parent(material, parent)
        save(material)

for name in ('Manny', 'Quinn'):
    mesh = load_required(COMMON + '/DBA/Meshes/SKM_' + name + '_Simple')
    # 物理资产只含两个模型共有的身体骨骼；保持既有碰撞配置，不改服务器胶囊。
    mesh.set_editor_property('physics_asset', physics)
    if name == 'Quinn':
        slots = list(mesh.get_editor_property('materials'))
        for index, slot in enumerate(slots):
            if not slot.material_interface:
                slot.material_interface = load_required(COMMON + '/DBA/Materials/Quinn/MI_Quinn_0' + str(index + 1))
        mesh.set_editor_property('materials', slots)
    save(mesh)

master = load_required('/DBAContentPack_Common/Materials/M_DBA_PrototypeCharacterColor')
master.set_editor_property('used_with_skeletal_mesh', True)
unreal.MaterialEditingLibrary.recompile_material(master)
save(master)

manifest = json.loads((ROOT / 'Tools/Unreal/Characters/ZodiacPlaceholderCharacters.json').read_text(encoding='utf-8-sig'))
for hero in manifest['heroes']:
    suffix = hero['heroId'].rsplit('.', 1)[1]
    profile = load_required('/' + hero['pack'] + '/Characters/DA_Appearance_Zodiac_' + suffix)
    # Python采用命名参数，明确只绕Z轴转向，不将模型横倒。
    profile.set_editor_property('mesh_relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
    save(profile)

# 开发Idle仅包含轻微胸部/颈部转动，禁止把UE4骨长或非均匀缩放写入UE5模型。
# 使用引擎IAnimationDataController制作真实动画；保留稳定AS/ABP路径及四秒循环。
sequence = load_required(COMMON + '/DBA/Animations/AS_DBA_PreviewIdle')
blueprint = load_required(COMMON + '/DBA/Animations/ABP_DBA_PreviewIdle')
if sequence.get_skeleton() != skeleton or blueprint.get_editor_property('target_skeleton') != skeleton:
    raise RuntimeError('先通过Monolith将Idle序列与动画蓝图绑定到完整UE5骨架。')
controller = sequence.controller
# UE5.8 Sequencer模型中的FK Rig也必须同步；仅改Skeleton字段会留下旧68骨树。
# 中立Editor命令由GamePlatformDeveloperTools注册，缺失时后续真实轨道写入失败并停止。
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(editor_world, 'GP.Animation.InitializeDataModel ' + sequence.get_path_name())
controller.open_bracket('重建UE5参考姿势开发Idle', should_transact=True)
try:
    controller.remove_all_bone_tracks()
    controller.set_frame_rate(unreal.FrameRate(30, 1))
    controller.set_number_of_frames(unreal.FrameNumber(120))
    for bone in unreal.AnimPoseExtensions.get_bone_names(reference_pose):
        transform = unreal.AnimPoseExtensions.get_ref_bone_pose(reference_pose, bone, unreal.AnimPoseSpaces.LOCAL)
        if not controller.add_bone_curve(bone):
            raise RuntimeError('创建Idle骨骼轨道失败: ' + str(bone))
        rotations = []
        # UE5骨骼局部X沿骨长方向；局部Yaw轻摆模拟呼吸，不通过缩放挤压蒙皮。
        amplitude = {'spine_03': 0.35, 'spine_04': 0.35, 'spine_05': 0.35, 'neck_01': -0.35}.get(str(bone), 0.0)
        for frame in range(121):
            angle = amplitude * math.sin(2.0 * math.pi * frame / 120.0)
            delta = unreal.Rotator(roll=0.0, pitch=0.0, yaw=angle).quaternion()
            rotations.append(unreal.MathLibrary.multiply_quat_quat(transform.rotation, delta))
        if not controller.set_bone_track_keys(bone, [transform.translation] * 121, rotations, [unreal.Vector(1.0, 1.0, 1.0)] * 121):
            raise RuntimeError('保存Idle骨骼关键帧失败: ' + str(bone))
finally:
    # 异常同样关闭通知括号；失败不保存序列，已保存材质可用执行前备份回退。
    controller.close_bracket()
sequence.set_editor_property('enable_root_motion', False)
save(sequence)
save(skeleton)
print(json.dumps({'savedAssets': changed}, ensure_ascii=False))
