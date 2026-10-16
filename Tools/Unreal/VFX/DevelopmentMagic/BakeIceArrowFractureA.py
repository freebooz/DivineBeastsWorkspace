#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM A 方案自然裂纹技术遮罩烘焙，仅由 Monolith 在所属 UE 编辑器执行。

通过引擎材质和 RenderTarget 生成不规则胞元边界，输出线性 R 通道遮罩；
它不是透明 Sprite 图片，也不包含完整技能、背景或辉光。烘焙材质只用于编辑器，
运行材质仅采样生成纹理，不承担胞元循环计算成本。拒绝覆盖已有资源。
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[4]
BASE = '/DBAPresentationPack_Core/VFX/IceArrow'
LIB = unreal.MaterialEditingLibrary


def prepare(resume=False):
    """先编译烘焙材质，再由引擎读取渲染目标生成真实 Texture2D 并保存源 PNG。"""
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    material_path = BASE + '/Materials/Review/M_DBA_IceArrow_FractureBake_A'
    texture_path = BASE + '/Textures/Review/T_DBA_IceArrow_Fracture_A'
    assert not unreal.EditorAssetLibrary.does_asset_exist(texture_path)
    name = material_path.rsplit('/', 1)[1]
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        assert resume, '只允许显式恢复本工具尚未产生纹理的失败候选'
        material = unreal.load_asset(material_path)
        LIB.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, material_path.rsplit('/', 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    uv = LIB.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate)
    field = LIB.create_material_expression(material, unreal.MaterialExpressionCustom)
    field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    custom_input = unreal.CustomInput()
    custom_input.set_editor_property('input_name', 'UV')
    field.set_editor_property('inputs', [custom_input])
    field.set_editor_property('description', '仅烘焙：不规则冰晶裂纹，运行时使用纹理')
    field.set_editor_property('code', '''
// 以相邻随机晶胞的距离差构造裂纹，避免规则网格；UV 无外部状态依赖。
float2 p = UV * float2(7.0, 5.0);
float2 cell = floor(p);
float2 local = frac(p);
float first = 100.0;
float second = 100.0;
for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
        float2 n = float2(x, y);
        float2 id = cell + n;
        float2 h = frac(sin(float2(dot(id, float2(127.1, 311.7)), dot(id, float2(269.5, 183.3)))) * 43758.5453);
        float2 delta = n + 0.18 + h * 0.64 - local;
        float d = dot(delta, delta);
        if (d < first) { second = first; first = d; }
        else { second = min(second, d); }
    }
}
float edge = sqrt(second) - sqrt(first);
return (1.0 - smoothstep(0.008, 0.035, edge));
''')
    assert LIB.connect_material_expressions(uv, '', field, 'UV')
    assert LIB.connect_material_property(field, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    print('ICE_FRACTURE_MATERIAL_READY_FOR_COMPILE')


def bake():
    """仅在外部 Monolith 材质编译完成后烘焙，避免首次采样未就绪的着色器。"""
    assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    texture_path = BASE + '/Textures/Review/T_DBA_IceArrow_Fracture_A'
    assert not unreal.EditorAssetLibrary.does_asset_exist(texture_path)
    material = unreal.load_asset(BASE + '/Materials/Review/M_DBA_IceArrow_FractureBake_A')
    assert material
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    target = unreal.RenderingLibrary.create_render_target2d(world, 1024, 1024, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    target.set_editor_property('target_gamma', 1.0)
    unreal.RenderingLibrary.draw_material_to_render_target(world, target, material)
    texture = unreal.RenderingLibrary.render_target_create_static_texture2d_editor_only(target, texture_path, unreal.TextureCompressionSettings.TC_MASKS)
    assert isinstance(texture, unreal.Texture2D)
    texture.set_editor_properties({'srgb': False, 'address_x': unreal.TextureAddress.TA_CLAMP, 'address_y': unreal.TextureAddress.TA_CLAMP})
    assert unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    source = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow/Review'
    source.mkdir(parents=True, exist_ok=True)
    unreal.RenderingLibrary.export_render_target(world, target, str(source), 'T_DBA_IceArrow_Fracture_A.png')
    for name in ['Crystal', 'Facet']:
        instance = unreal.load_asset(BASE + '/Materials/Review/MI_DBA_IceArrow_' + name + '_A')
        LIB.set_material_instance_texture_parameter_value(instance, 'CrackMask', texture)
        LIB.update_material_instance(instance)
        assert unreal.EditorAssetLibrary.save_loaded_asset(instance, False)
    print('ICE_FRACTURE_BAKED', texture.get_path_name())


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bake', action='store_true')
    parser.add_argument('--resume', action='store_true')
    options = parser.parse_args()
    bake() if options.bake else prepare(options.resume)
