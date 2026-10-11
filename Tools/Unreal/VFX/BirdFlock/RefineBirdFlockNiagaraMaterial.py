#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""修正通用母材质的振翅时钟及Niagara逐粒子随机相位，仅操作本任务已有母材质。
原图输入已经乘2π，Sine必须Period=2π，避免2Hz被误放大到12.56Hz。
兼容旧ISM预览：随机相位=PerInstanceRandom+ParticleRandom，两个渲染后端共享母版。
经Monolith run_python真实编辑与保存，不创建替代空白纹理或重置已有图。
"""
import unreal
PATH='/GamePlatformVFX/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master'
a=unreal.EditorAssetLibrary
m=a.load_asset(PATH)
assert isinstance(m,unreal.Material)
sine=unreal.find_object(m,'MaterialExpressionSine_0')
instance=unreal.find_object(m,'MaterialExpressionPerInstanceRandom_0')
phase=unreal.find_object(m,'MaterialExpressionMultiply_3')
assert isinstance(sine,unreal.MaterialExpressionSine)
assert isinstance(instance,unreal.MaterialExpressionPerInstanceRandom)
assert isinstance(phase,unreal.MaterialExpressionMultiply)
lib=unreal.MaterialEditingLibrary
sine.set_editor_property('period',6.28318530718)
particle=unreal.find_object(m,'MaterialExpressionParticleRandom_0')
if particle is None:
    particle=lib.create_material_expression(m,unreal.MaterialExpressionParticleRandom,-1250,990)
    particle.set_editor_property('desc','Niagara逐鸟随机振翅相位')
combined=unreal.find_object(m,'MaterialExpressionAdd_1')
if combined is None:
    combined=lib.create_material_expression(m,unreal.MaterialExpressionAdd,-950,1010)
    combined.set_editor_property('desc','ISM与Niagara随机相位兼容')
assert isinstance(combined,unreal.MaterialExpressionAdd)
assert lib.connect_material_expressions(instance,'',combined,'A')
assert lib.connect_material_expressions(particle,'',combined,'B')
assert lib.connect_material_expressions(combined,'',phase,'A')
messages=list(lib.recompile_material(m))
assert not any('error' in str(x).lower() for x in messages),messages
assert a.save_loaded_asset(m,only_if_is_dirty=False)
print('BIRD_MATERIAL_REFINED',sine.get_editor_property('period'),particle.get_path_name(),m.get_path_name())
