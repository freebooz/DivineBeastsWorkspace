#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 玄霜棱矢候选切面材质修订，由 Monolith 在 UE 编辑器执行。

只调整 Review 中已有母材质的发光通道，保留实际三维法线、光照、透明度与实例合同。
采用随视角变化的艺术化环境高光提升暗场可读性；不是物理环境反射替代品，
也不读取或描绘整张概念图。正式母材质、玩法权威和内容映射均不受影响。
重复调用拒绝添加第二套节点；材质编译、性能和实机截图由调用方独立核验。
"""
import unreal

BASE = '/DBAPresentationPack_Core/VFX/IceArrow/Materials/Review/'
LIB = unreal.MaterialEditingLibrary


def refine():
    """添加廉价三维切面响应，削弱网格状裂纹；失败保留候选供诊断，不触碰正式资产。"""
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    material = unreal.load_asset(BASE + 'M_DBA_IceArrow_Crystal_A')
    expressions = LIB.get_material_expressions(material)
    assert not any(e.get_editor_property('desc') == '玄霜切面视角响应' for e in expressions)
    crack = next(e for e in expressions if isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D)
                 and str(e.get_editor_property('parameter_name')) == 'CrackMask')
    normal = LIB.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -700, 1150)
    view = LIB.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -700, 1300)
    reflection = LIB.create_material_expression(material, unreal.MaterialExpressionCustom, -350, 1150)
    reflection.set_editor_property('desc', '玄霜切面视角响应')
    reflection.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in ['N', 'V', 'Crack']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    reflection.set_editor_property('inputs', inputs)
    reflection.set_editor_property('code', '''
// 使用真实网格面法线构成深蓝暗面与窄高光；裂纹只作次要细节。
float3 n = normalize(N);
float3 v = normalize(V);
float3 r = reflect(-v, n);
float face = saturate(dot(n, normalize(float3(-0.3, 0.35, 0.85))));
float key = pow(saturate(dot(r, normalize(float3(-0.3, 0.2, 0.93)))), 28.0);
float rim = pow(saturate(dot(r, normalize(float3(0.1, -0.95, 0.3)))), 16.0);
float edge = pow(1.0 - saturate(abs(dot(n, v))), 4.5);
float3 body = lerp(float3(0.002, 0.008, 0.025), float3(0.014, 0.08, 0.20), face);
return body + key * float3(1.3, 2.0, 2.8) + rim * float3(0.35, 0.9, 1.7)
       + edge * float3(0.05, 0.24, 0.50) + Crack * float3(0.018, 0.06, 0.12);
''')
    for source, pin, target in [(normal, '', 'N'), (view, '', 'V'), (crack, 'R', 'Crack')]:
        assert LIB.connect_material_expressions(source, pin, reflection, target)
    assert LIB.connect_material_property(reflection, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    material.set_editor_property('two_sided', False)
    LIB.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    for name in ['Crystal', 'Facet']:
        instance = unreal.load_asset(BASE + 'MI_DBA_IceArrow_' + name + '_A')
        LIB.set_material_instance_scalar_parameter_value(instance, 'CrystalOpacity', .86)
        LIB.set_material_instance_vector_parameter_value(instance, 'IceTint', unreal.LinearColor(.014, .055, .12, 1))
        LIB.update_material_instance(instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    print('ICE_FACET_RESPONSE_UPDATED')


if __name__ == '__main__':
    refine()
