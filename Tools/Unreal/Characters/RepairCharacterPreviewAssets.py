# -*- coding: utf-8 -*-
"""开发角色预览资源修复，由锁定UE编辑器/Monolith执行。
只恢复公共Mannequin的既定骨架、物理资产、母材质及迁移丢失的纹理引用，
并修正十二个外观旋转；不改角色权威定义、不生成UI、不删除资产。
执行前须备份相关资产；失败保留已保存项，通过备份可逐文件回退。
"""
import json
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

skeleton = load_required(COMMON + '/DBA/Meshes/SK_Mannequin_Skeleton')
physics = load_required(COMMON + '/DBA/Meshes/SK_Mannequin_PhysicsAsset')
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
    # 两个网格原本就共享同一Mannequin骨架/物理资产，恢复引用不改骨架结构。
    # Skeleton是Python只读字段：先由Monolith set_property_at_path恢复引用。
    # 本脚本验证既定骨架，拒绝绕过引擎访问限制或静默继续。
    if mesh.get_editor_property('skeleton') != skeleton:
        raise RuntimeError('先通过Monolith恢复网格Skeleton引用: ' + mesh.get_path_name())
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
print(json.dumps({'savedAssets': changed}, ensure_ascii=False))
