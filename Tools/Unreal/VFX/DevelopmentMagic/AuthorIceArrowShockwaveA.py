#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 候选薄环材质，仅由 Monolith 原生创建；不参与碰撞或权威范围显示。

项目表现包拥有材质与实例。解析式圆环无需完整效果贴图，厚度由UV距离构造；
尺寸、方向、生命周期由独立Niagara Mesh发射器管理。拒绝覆盖同名资产。
"""
import unreal


def run():
    """创建参数化薄环，并保存可引用的实例；外部继续执行真实着色器编译审核。"""
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    path = '/DBAPresentationPack_Core/VFX/IceArrow/Materials/Review'
    name = 'M_DBA_IceArrow_Shockwave_A'
    assert not unreal.EditorAssetLibrary.does_asset_exist(path+'/'+name)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lib = unreal.MaterialEditingLibrary
    material = tools.create_asset(name,path,unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_properties({'shading_model':unreal.MaterialShadingModel.MSM_UNLIT,
                                   'blend_mode':unreal.BlendMode.BLEND_ADDITIVE,'two_sided':True})
    lib.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True)
    uv=lib.create_material_expression(material,unreal.MaterialExpressionTextureCoordinate)
    particle=lib.create_material_expression(material,unreal.MaterialExpressionParticleColor)
    field=lib.create_material_expression(material,unreal.MaterialExpressionCustom)
    parameter=unreal.CustomInput();parameter.set_editor_property('input_name','UV')
    field.set_editor_properties({'inputs':[parameter],'output_type':unreal.CustomMaterialOutputType.CMOT_FLOAT1,
        'code':'float radial=length(UV-0.5); return (1-smoothstep(0.002,0.008,abs(radial-0.43)))+0.3*(1-smoothstep(0.001,0.004,abs(radial-0.405)));'})
    assert lib.connect_material_expressions(uv,'',field,'UV')
    alpha=lib.create_material_expression(material,unreal.MaterialExpressionMultiply)
    assert lib.connect_material_expressions(field,'',alpha,'A')
    assert lib.connect_material_expressions(particle,'A',alpha,'B')
    tint=lib.create_material_expression(material,unreal.MaterialExpressionVectorParameter)
    tint.set_editor_properties({'parameter_name':'IceTint','default_value':unreal.LinearColor(.6,2.5,4,1)})
    assert lib.connect_material_property(tint,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
    instance=tools.create_asset('MI_DBA_IceArrow_Shockwave_A',path,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(instance,material)
    lib.update_material_instance(instance)
    assert unreal.EditorAssetLibrary.save_loaded_asset(instance,False)
    print('ICE_SHOCKWAVE_READY_FOR_COMPILE')


if __name__=='__main__':
    run()
