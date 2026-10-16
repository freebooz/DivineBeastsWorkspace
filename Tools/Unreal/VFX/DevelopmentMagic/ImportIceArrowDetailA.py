#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 冰晶独立裂隙遮罩导入与能量材质候选，由 Monolith 执行。

项目表现层拥有纹理和材质；遮罩仅表示局部冰内结构，不能代替三维技能。
源图是无透明要求的黑白线性遮罩，不宣称为透明 Sprite。材质复用一个参数化母版，
分别供核心、近身流线与历史 Ribbon 使用；粒子 Alpha 负责生命周期。
只写 Review 资源，已有同名资产拒绝覆盖。调用方须独立检查编译和实际截图。
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
BASE = '/DBAPresentationPack_Core/VFX/IceArrow'
LIB = unreal.MaterialEditingLibrary


def run():
    """原生导入遮罩，创建三维粒子能量材质，并重接现有候选实例。"""
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    name = 'T_DBA_IceArrow_InternalFracture_A'
    texture_path = BASE + '/Textures/Review/' + name
    source = ROOT / ('Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow/Review/' + name + '.png')
    assert source.exists() and not unreal.EditorAssetLibrary.does_asset_exist(texture_path)
    task = unreal.AssetImportTask()
    task.set_editor_properties({'filename': str(source), 'destination_path': BASE + '/Textures/Review',
                               'destination_name': name, 'automated': True, 'replace_existing': False, 'save': False})
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(texture_path)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_properties({'srgb': False, 'compression_settings': unreal.TextureCompressionSettings.TC_MASKS,
                                  'max_texture_size': 1024, 'address_x': unreal.TextureAddress.TA_WRAP,
                                  'address_y': unreal.TextureAddress.TA_WRAP})
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    for part in ['Crystal', 'Facet']:
        instance = unreal.load_asset(BASE + '/Materials/Review/MI_DBA_IceArrow_' + part + '_A')
        LIB.set_material_instance_texture_parameter_value(instance, 'CrackMask', texture)
        LIB.update_material_instance(instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    crystal = unreal.load_asset(BASE + '/Materials/Review/M_DBA_IceArrow_Crystal_A')
    reflection = next(e for e in LIB.get_material_expressions(crystal) if e.get_editor_property('desc') == '玄霜切面视角响应')
    code = reflection.get_editor_property('code').replace('key * float3(1.3, 2.0, 2.8)', 'key * float3(0.45, 0.8, 1.2)')
    code = code.replace('Crack * float3(0.018, 0.06, 0.12)', 'pow(saturate(Crack), 1.8) * float3(0.35, 0.65, 1.1)')
    reflection.set_editor_property('code', code)
    LIB.recompile_material(crystal)
    assert unreal.EditorAssetLibrary.save_loaded_asset(crystal, False)
    material_name = 'M_DBA_IceArrow_Energy_A'
    path = BASE + '/Materials/Review'
    assert not unreal.EditorAssetLibrary.does_asset_exist(path + '/' + material_name)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(material_name, path, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_properties({'shading_model': unreal.MaterialShadingModel.MSM_UNLIT,
                                    'blend_mode': unreal.BlendMode.BLEND_ADDITIVE, 'two_sided': True})
    for usage in [unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES, unreal.MaterialUsage.MATUSAGE_NIAGARA_RIBBONS]:
        LIB.set_base_material_usage(material, usage, True)

    def scalar(parameter, default):
        """实例标量：强度、遮罩模式与密度均可调，不硬编码粒子寿命。"""
        node = LIB.create_material_expression(material, unreal.MaterialExpressionScalarParameter)
        node.set_editor_properties({'parameter_name': parameter, 'default_value': default})
        return node

    uv = LIB.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate)
    tex = LIB.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D)
    tex.set_editor_properties({'parameter_name': 'DetailMask', 'texture': texture,
                               'sampler_type': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS})
    particle = LIB.create_material_expression(material, unreal.MaterialExpressionParticleColor)
    tint = LIB.create_material_expression(material, unreal.MaterialExpressionVectorParameter)
    tint.set_editor_properties({'parameter_name': 'IceTint', 'default_value': unreal.LinearColor(.2, .7, 1.0, 1)})
    strength, opacity, mode = scalar('EmissiveStrength', 4), scalar('OpacityScale', .6), scalar('FlowMode', 0)
    field = LIB.create_material_expression(material, unreal.MaterialExpressionCustom)
    fields = []
    for key in ['UV', 'Detail', 'Mode']:
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); fields.append(item)
    field.set_editor_properties({'inputs': fields, 'output_type': unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                                'description': '流线为平滑横截面，核心为局部冰内细节，生命周期由粒子控制',
                                'code': 'float d=abs(UV.y-0.5)*2.0; float band=exp2(-d*d*24.0); return lerp(saturate(Detail*1.6+band*0.12), band, saturate(Mode));'})
    for source_node, pin, target in [(uv, '', 'UV'), (tex, 'R', 'Detail'), (mode, '', 'Mode')]:
        assert LIB.connect_material_expressions(source_node, pin, field, target)

    def multiply(a, b, ap='', bp=''):
        """显式连接并检查乘法节点，避免Alpha或颜色被误接为常数。"""
        result = LIB.create_material_expression(material, unreal.MaterialExpressionMultiply)
        assert LIB.connect_material_expressions(a, ap, result, 'A')
        assert LIB.connect_material_expressions(b, bp, result, 'B')
        return result

    emission = multiply(multiply(particle, tint, 'RGB', 'RGB'), strength)
    alpha = multiply(multiply(field, particle, '', 'A'), opacity)
    assert LIB.connect_material_property(emission, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    for name, flow_mode, power, density, color in [('Core', 0, 7, .7, (.5, .85, 1.3, 1)), ('Flow', 1, 5, .5, (.14, .55, 1.0, 1))]:
        instance = unreal.load_asset(path + '/MI_DBA_IceArrow_' + name + '_A')
        LIB.set_material_instance_parent(instance, material)
        for parameter, value in [('FlowMode', flow_mode), ('EmissiveStrength', power), ('OpacityScale', density)]:
            LIB.set_material_instance_scalar_parameter_value(instance, parameter, value)
        LIB.set_material_instance_vector_parameter_value(instance, 'IceTint', unreal.LinearColor(*color))
        LIB.update_material_instance(instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    print('ICE_DETAIL_AND_ENERGY_READY_FOR_REVIEW')


if __name__ == '__main__':
    run()

