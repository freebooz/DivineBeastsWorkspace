# -*- coding: utf-8 -*-
"""通过Monolith增量调整新手村雪次表面颜色；只编辑现有材质，不保存地图。

项目层作者脚本沿用平台PBR配方的参数合同。输入为用户最新色板的线性RGB，
仅替换Albedo到SubsurfaceColor支路的常量，不改变扫描BaseColor或反射标量。
"""
import unreal

mat = unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
lib = unreal.MaterialEditingLibrary
nodes = list(lib.get_material_expressions(mat))
name = 'VillageSnow Subsurface Tint'
tint = next((n for n in nodes if isinstance(n, unreal.MaterialExpressionVectorParameter)
             and str(n.get_editor_property('parameter_name')) == name), None)
if tint is None:
    # 用原生连接关系识别唯一SSS乘法，避免依赖节点编号或错误修改基础色。
    albedo = next(n for n in nodes if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)
                  and str(n.get_editor_property('parameter_name')) == 'VillageSnowAlbedoTexture')
    matches = []
    for n in nodes:
        if not isinstance(n, unreal.MaterialExpressionMultiply):
            continue
        inputs = list(lib.get_inputs_for_material_expression(mat, n))
        constants = [v for v in inputs if isinstance(v, unreal.MaterialExpressionConstant3Vector)]
        if albedo in inputs and len(constants) == 1:
            c = constants[0].get_editor_property('constant')
            if abs(c.r-.32) < .00001 and abs(c.g-.42) < .00001 and abs(c.b-.55) < .00001:
                matches.append((n, constants[0]))
    if len(matches) != 1:
        raise RuntimeError('无法唯一识别原雪次表面颜色乘法，停止以保护其他输出')
    multiply, old = matches[0]
    tint = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -850, 5150)
    tint.set_editor_property('parameter_name', name)
    tint.set_editor_property('group', 'Snow Subsurface')
    if not lib.connect_material_expressions(tint, '', multiply, 'B'):
        raise RuntimeError('浅蓝参数连接到雪Albedo乘法失败')
    # 原常量只属于此SSS分支；删除已断开的作者节点，不改变其他节点。
    lib.delete_material_expression(mat, old)
tint.set_editor_property('default_value', unreal.LinearColor(.547744, .710151, 1., 1.))
tint.set_editor_property('desc', '用户参考色板：线性RGB，Albedo乘色后仅进入积雪次表面分支')
lib.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
    raise RuntimeError('雪次表面浅蓝参数保存失败')
print('SNOW_SUBSURFACE_TINT_SAVED linear=(0.547744,0.710151,1.0) sRGB=#C4DCFF')
