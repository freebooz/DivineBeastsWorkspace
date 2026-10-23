# -*- coding: utf-8 -*-
"""Monolith材质作者：复用同一扫描的高度/法线多尺度细节，不生成噪点图片或额外世界状态。"""
import unreal
m=unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
lib=unreal.MaterialEditingLibrary
nodes={n.get_name():n for n in lib.get_material_expressions(m)}
if any(n.get_editor_property('desc')=='GP.Snow.NoisyHeight' for n in nodes.values()):raise RuntimeError('噪声层已存在，请增量调整参数')
base_height=next(n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='VillageSnowHeightTexture')
base_normal=nodes['MaterialExpressionTextureSampleParameter2D_2']
uv=nodes['MaterialExpressionMultiply_1']
def node(cls,x,y):return lib.create_material_expression(m,cls,x,y)
def wire(a,b,pin,out=''):
    result=lib.connect_material_expressions(a,out,b,pin)
    if not result and pin in ('Input','Coordinates','VectorInput'):result=lib.connect_material_expressions(a,out,b,'')
    if not result:raise RuntimeError('噪声层连线失败: '+b.get_name()+'.'+pin)
def scalar(name,v,x,y):
    n=node(unreal.MaterialExpressionScalarParameter,x,y);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',v);return n
def const(v,x,y):
    n=node(unreal.MaterialExpressionConstant,x,y);n.set_editor_property('r',v);return n
def op(cls,a,b,x,y,out_a='',out_b=''):
    n=node(cls,x,y);wire(a,n,'A',out_a);wire(b,n,'B',out_b);return n
def sample(original,coords,x,y):
    n=node(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
    for p in ['parameter_name','texture','sampler_type']:n.set_editor_property(p,original.get_editor_property(p))
    wire(coords,n,'Coordinates');return n
density=scalar('Snow Noise Density',12.,-2700,6200)
fine_density=scalar('Snow Fine Noise Density',48.,-2700,6450)
strength=scalar('Snow Noise Strength',.30,-2700,6700)
u1=op(unreal.MaterialExpressionMultiply,uv,density,-2400,6200)
u2=op(unreal.MaterialExpressionMultiply,uv,fine_density,-2400,6450)
h1=sample(base_height,u1,-2150,6200);h2=sample(base_height,u2,-2150,6500)
n1=sample(base_normal,u1,-2150,6820);n2=sample(base_normal,u2,-2150,7130)
c1=op(unreal.MaterialExpressionSubtract,h1,const(.5,-2120,7400),-1790,6250,'R')
c2=op(unreal.MaterialExpressionSubtract,h2,const(.5,-2120,7550),-1790,6520,'R')
band=op(unreal.MaterialExpressionAdd,c1,op(unreal.MaterialExpressionMultiply,c2,const(.33,-1790,7450),-1540,6550),-1250,6250)
perturb=op(unreal.MaterialExpressionMultiply,band,strength,-990,6250)
noisy_height=node(unreal.MaterialExpressionSaturate,-440,6250)
wire(op(unreal.MaterialExpressionAdd,base_height,perturb,-720,6250,'R'),noisy_height,'Input')
noisy_height.set_editor_property('desc','GP.Snow.NoisyHeight')
# 同一细节高度和配套扫描法线一起变化；雪面细粒不依赖发光或透明叠片。
detail_n=op(unreal.MaterialExpressionAdd,base_normal,op(unreal.MaterialExpressionMultiply,n1,strength,-1500,6900),-1200,6900)
detail_n=op(unreal.MaterialExpressionAdd,detail_n,op(unreal.MaterialExpressionMultiply,n2,const(.10,-1790,7600),-1500,7200),-920,6930)
normal=node(unreal.MaterialExpressionNormalize,-630,6900);wire(detail_n,normal,'VectorInput')
wire(normal,nodes['MaterialExpressionLinearInterpolate_5'],'B')
for n in nodes.values():
    if isinstance(n,unreal.MaterialExpressionBumpOffset):wire(noisy_height,n,'Height')
# Displacement制作入口从该有标签高度读取，仍由参数5控制厘米幅度。
rough_clamp=next(n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionClamp))
orm=next(n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='VillageSnowORMTexture')
rough_var=op(unreal.MaterialExpressionMultiply,c2,const(.10,-700,7400),-430,7370)
wire(op(unreal.MaterialExpressionAdd,orm,rough_var,-170,7340,'G'),rough_clamp,'Input')
lib.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False):raise RuntimeError('噪声材质保存失败')
print('SNOW_NOISE_AUTHORED density=12 fine_density=48 strength=.30 paired normal/height/roughness')
