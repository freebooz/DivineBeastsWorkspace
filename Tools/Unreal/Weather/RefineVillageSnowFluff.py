# -*- coding: utf-8 -*-
"""Monolith编辑器增量制作：连续松雪体积、细颗粒与独立5cm压雪深度。

本脚本只修改项目雪母材质，不修改地图、碰撞、出生点、平台运行机制或扫描原图。
使用原有采样、Nanite置换及局部脚印RT，重复运行按标记更新，不复制资源。
"""
import unreal

lib = unreal.MaterialEditingLibrary
mat = unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
    raise RuntimeError('请先结束PIE，以免修改运行中的雪材质')
nodes = {n.get_name(): n for n in lib.get_material_expressions(mat)}
params = {str(n.get_editor_property('parameter_name')): n for n in nodes.values()
          if isinstance(n, unreal.MaterialExpressionScalarParameter)}


def wire(source, target, pin, output=''):
    """连接原生图；失败停止，不能保存只有参数没有实际输出的假配置。"""
    if not lib.connect_material_expressions(source, output, target, pin):
        raise RuntimeError('松雪材质接线失败: ' + target.get_name() + '.' + pin)


def scalar(name, value, y):
    """新增参数仅管理表现厘米合同，服务器碰撞和地形高度不变。"""
    n = params.get(name)
    if n is None:
        n = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -2600, y)
        n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    return n


def custom(tag, code, inputs, y):
    """复用带稳定描述标记的数学节点，不增加纹理采样或透明层。"""
    n = next((n for n in nodes.values() if n.get_editor_property('desc') == tag), None)
    if n is None:
        n = lib.create_material_expression(mat, unreal.MaterialExpressionCustom, -900, y)
    n.set_editor_property('desc', tag)
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    rows = []
    for name in inputs:
        entry = unreal.CustomInput()
        entry.set_editor_property('input_name', name)
        rows.append(entry)
    n.set_editor_property('inputs', rows)
    for name, (source, output) in inputs.items():
        wire(source, n, name, output)
    return n


# 原高度直接乘5只得到平均约2cm的薄层。用同一扫描的圆滑高度形成连续底层，
# 高频噪声仍保留，但限制对轮廓的扰动，避免碎冰/沙粒形状。
base = next(n for n in nodes.values() if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)
            and str(n.get_editor_property('parameter_name')) == 'VillageSnowHeightTexture')
noise = next(n for n in nodes.values() if n.get_editor_property('desc') == 'GP.Snow.NoisyHeight')
profile = custom('GP.Snow.FluffyHeight',
    'float h=smoothstep(0.10,0.90,Base); return saturate(0.56+0.40*h+0.04*(Detail-Base));',
    {'Base': (base, 'R'), 'Detail': (noise, '')}, 7900)
depth = scalar('Snow Footprint Depth Cm', 5., 8370)
thickness = params['Displacement Multiplier']
thickness.set_editor_property('default_value', 10.)
params['VillageSnowNormalStrength'].set_editor_property('default_value', .38)
params['VillageSnowAlbedoDetailContrast'].set_editor_property('default_value', .50)
params['VillageSnowParallaxHeightRatio'].set_editor_property('default_value', .004)

# 找到原脚印高度/粗糙度节点，维持同一个RT来源和世界UV，不创建第二条接收链。
relief = next(n for n in nodes.values() if isinstance(n, unreal.MaterialExpressionCustom)
              and ('Height*(1-saturate(Field.r)*0.96)' in n.get_editor_property('code')
                   or n.get_editor_property('desc') == 'GP.Snow.CompactedHeight'))
old_inputs = list(relief.get_editor_property('inputs'))
field_input = next(i for i in old_inputs if str(i.get_editor_property('input_name')) == 'Field')
# 保留CustomInput中的原ExpressionInput（RT边界遮罩），只补充深度与层厚。
for name in ('DepthCm', 'ThicknessCm'):
    if not any(str(i.get_editor_property('input_name')) == name for i in old_inputs):
        entry = unreal.CustomInput(); entry.set_editor_property('input_name', name); old_inputs.append(entry)
relief.set_editor_property('inputs', old_inputs)
relief.set_editor_property('desc', 'GP.Snow.CompactedHeight')
relief.set_editor_property('code', '''float thickness=clamp(ThicknessCm,0.0,10.0);
float depth=min(clamp(DepthCm,0.0,5.0),thickness)/max(thickness,0.001);
float pressure=saturate(Field.r);
return saturate(max(Height-depth*pressure,0)+Field.g*0.035*(1-pressure));''')
wire(profile, relief, 'Height')
wire(depth, relief, 'DepthCm')
wire(thickness, relief, 'ThicknessCm')
rough = next(n for n in nodes.values() if isinstance(n, unreal.MaterialExpressionCustom)
             and (n.get_editor_property('desc') == 'GP.Snow.CompactedRoughness'
                  or 'Roughness-Field.r' in n.get_editor_property('code')
                  or 'max(Roughness,0.86)-Field.r' in n.get_editor_property('code')))
rough.set_editor_property('desc', 'GP.Snow.CompactedRoughness')
rough.set_editor_property('code', 'return clamp(max(Roughness,0.86)-Field.r*0.06,0.80,0.96);')

# 原压痕法线系数5cm不随10cm松雪层放大，防止出现视觉深度与实际置换不符。
lib.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
    raise RuntimeError('松雪材质保存失败')
print('VILLAGE_FLUFFY_SNOW_SAVED max_thickness_cm=10 footprint_depth_cm=5 no_new_samplers=true')
