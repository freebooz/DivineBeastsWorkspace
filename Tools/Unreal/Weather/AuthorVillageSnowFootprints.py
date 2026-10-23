# -*- coding: utf-8 -*-
"""Monolith作者：平台双缓冲压雪更新材质，以及项目现有雪母材质的局部接收支路。

不修改地图、高度图、碰撞或动画资产。足印纹理由真实客户端RT生成；图中默认黑图仅用于安全回退。
"""
import unreal
if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
    raise RuntimeError('编辑器正在PIE验证，保留当前会话，待结束后再制作并保存材质')
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
black=unreal.load_asset('/Engine/EngineResources/Black')
if not isinstance(black,unreal.Texture2D):raise RuntimeError('缺少引擎中性黑纹理，停止以免未绑定时污染雪面')
sampler=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if black.get_editor_property('srgb') else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
def node(m,cls,x,y):return lib.create_material_expression(m,cls,x,y)
def wire(a,b,pin,out=''):
    ok=lib.connect_material_expressions(a,out,b,pin)
    if not ok and pin in ('Input','Coordinates','VectorInput'):ok=lib.connect_material_expressions(a,out,b,'')
    if not ok:raise RuntimeError('脚印连线失败: '+b.get_name()+'.'+pin)
def parameter(m,name,value,x,y):
    n=node(m,unreal.MaterialExpressionVectorParameter,x,y);n.set_editor_property('parameter_name',name)
    n.set_editor_property('default_value',unreal.LinearColor(*value));return n
def scalar(m,name,value,x,y):
    n=node(m,unreal.MaterialExpressionScalarParameter,x,y);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def custom(m,code,inputs,x,y,scalar_output=False):
    n=node(m,unreal.MaterialExpressionCustom,x,y);n.set_editor_property('code',code)
    n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1 if scalar_output else unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    rows=[]
    for name in inputs:
        entry=unreal.CustomInput();entry.set_editor_property('input_name',name);rows.append(entry)
    n.set_editor_property('inputs',rows)
    for name,value in inputs.items():wire(value,n,name)
    return n
update_path='/GamePlatformSurface/Materials/Snow/M_GP_SnowContactUpdate'
update=unreal.load_asset(update_path)
if not update:
    update=tools.create_asset('M_GP_SnowContactUpdate','/GamePlatformSurface/Materials/Snow',unreal.Material,unreal.MaterialFactoryNew())
    update.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    uv=node(update,unreal.MaterialExpressionTextureCoordinate,-1500,0)
    previous=node(update,unreal.MaterialExpressionTextureObjectParameter,-1500,180)
    previous.set_editor_property('parameter_name','PreviousField');previous.set_editor_property('texture',black)
    previous.set_editor_property('sampler_type',sampler)
    offset=parameter(update,'PreviousOffset',(0,0,0,0),-1500,400)
    decay=scalar(update,'DecayAmount',0.,-1500,600);count=scalar(update,'ContactCount',0.,-1500,800)
    inputs={'UV':uv,'PreviousField':previous,'Offset':offset,'Decay':decay,'Count':count}
    for i in range(16):
        a=parameter(update,'ContactA'+str(i),(0,0,1,0),-1500,1050+i*230)
        # VectorParameter默认RGB输出不包含方向Y；显式Append alpha形成float4。
        a4=node(update,unreal.MaterialExpressionAppendVector,-1100,1050+i*230)
        wire(a,a4,'A');wire(a,a4,'B','A')
        b=parameter(update,'ContactB'+str(i),(.01,.005,0,0),-800,1050+i*230)
        inputs['A'+str(i)]=a4;inputs['B'+str(i)]=b
    code='''float2 oldUV=UV+Offset.xy;
float valid=step(0,oldUV.x)*step(oldUV.x,1)*step(0,oldUV.y)*step(oldUV.y,1);
float2 value=max(Texture2DSample(PreviousField,PreviousFieldSampler,saturate(oldUV)).rg*valid-Decay,0);
float4 a[16]={'''+','.join('A'+str(i) for i in range(16))+'''};
float3 b[16]={'''+','.join('B'+str(i) for i in range(16))+'''};
[loop] for(int i=0;i<min((int)Count,16);++i){
 float2 d=UV-a[i].xy;
 float2 q=float2(dot(d,a[i].zw)/max(b[i].x,0.00001),dot(d,float2(-a[i].w,a[i].z))/max(b[i].y,0.00001));
 // 前掌略宽、后跟略窄，连续椭圆鞋底，边缘连续平滑。
 q.y*=lerp(1.12,0.92,saturate(q.x*0.5+0.5));
 float r=length(q);
 float press=(1-smoothstep(0.70,1.0,r))*b[i].z;
 float rim=smoothstep(0.88,1.02,r)*(1-smoothstep(1.02,1.28,r));
 value=max(value,float2(press,rim));
}
return float3(saturate(value),0);'''
    out=custom(update,code,inputs,200,500)
    if not lib.connect_material_property(out,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR):raise RuntimeError('RT更新材质输出失败')
lib.recompile_material(update)
if not unreal.EditorAssetLibrary.save_loaded_asset(update,only_if_is_dirty=False):raise RuntimeError('RT更新材质保存失败')

m=unreal.load_asset('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
nodes={n.get_name():n for n in lib.get_material_expressions(m)}
if any(isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='SnowContactTexture' for n in nodes.values()):
    raise RuntimeError('脚印支路已存在，需增量检查而非重复生成')
world=nodes['MaterialExpressionWorldPosition_0']
origin=parameter(m,'SnowContactOriginExtent',(0,0,.000625,0),-2700,8100)
uv_code='float2 uv=(Position.xy-Origin.xy)*Origin.z; return float3(uv,0);'
uv3=custom(m,uv_code,{'Position':world,'Origin':origin},-2300,8100)
uv_mask=node(m,unreal.MaterialExpressionComponentMask,-2000,8100);uv_mask.set_editor_property('r',True);uv_mask.set_editor_property('g',True);wire(uv3,uv_mask,'Input')
field=node(m,unreal.MaterialExpressionTextureSampleParameter2D,-1700,8100)
field.set_editor_property('parameter_name','SnowContactTexture');field.set_editor_property('texture',black);field.set_editor_property('sampler_type',sampler);wire(uv_mask,field,'Coordinates')
# 贴图Clamp边界之外的世界位置强制归零，不将边沿足迹平铺至全地图。
mask=custom(m,'float v=step(0,UV.x)*step(UV.x,1)*step(0,UV.y)*step(UV.y,1); return Field.rgb*v;',{'UV':uv_mask,'Field':field},-1300,8100)
height=next(n for n in nodes.values() if n.get_editor_property('desc')=='GP.Snow.NoisyHeight')
relief=custom(m,'return saturate(Height*(1-saturate(Field.r)*0.96)+Field.g*0.055*(1-saturate(Field.r)));',{'Height':height,'Field':mask},-900,8100,True)
# Custom返回float3，为高度取R；最大雪面始终受原Displacement=5合同限制。
wire(relief,nodes['MaterialExpressionMultiply_14'],'A')
rough=next(n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionClamp))
rough_mix=custom(m,'return clamp(Roughness-Field.r*0.10,0.60,0.96);',{'Roughness':rough,'Field':mask},-900,8440,True)
wire(rough_mix,nodes['MaterialExpressionLinearInterpolate_3'],'B')
# 有限差分将足印边缘加入切线空间法线；ddx/ddy使用真实世界梯度，雪外由原覆盖分支归零。
normal=nodes['MaterialExpressionNormalize_1']
coverage=nodes['MaterialExpressionSaturate_3']
normal_mix=custom(m,'''float h=Field.r-Field.g*0.055;
float3 dpdx=ddx(Position),dpdy=ddy(Position);
float2 grad=float2(ddx(h),ddy(h));
float3 n=normalize(Normal);
float det=dpdx.x*dpdy.y-dpdx.y*dpdy.x;
float2 worldGrad=abs(det)>0.00001?float2(grad.x*dpdy.y-grad.y*dpdx.y,dpdx.x*grad.y-dpdy.x*grad.x)/det:float2(0,0);
return normalize(n+float3(worldGrad*5.0*Coverage,0));''',{'Field':mask,'Position':world,'Normal':normal,'Coverage':coverage},-900,8800)
if not lib.connect_material_property(normal_mix,'',unreal.MaterialProperty.MP_NORMAL):raise RuntimeError('足印法线输出失败')
lib.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m,only_if_is_dirty=False):raise RuntimeError('脚印接收材质保存失败')
print('SNOW_FOOTPRINT_MATERIALS_SAVED native RT update + displacement/roughness/normal receiver; source map untouched')
