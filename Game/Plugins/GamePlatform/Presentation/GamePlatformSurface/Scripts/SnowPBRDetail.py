# -*- coding: utf-8 -*-
"""编辑器制作配方：有界近景雪视差与配套PBR输出；不持有天气/资源/世界状态。

共享中性地形桥可展开这些数学节点，避免硬引用ClientOnly平台函数和贴图。
所有资源由调用方提供，参数前缀由适配层传入；平台不认识项目与MOBA。
"""
import unreal


def build_detail(mat, uv, position, coverage, albedo, normal, fallback, prefix='Snow'):
    lib=unreal.MaterialEditingLibrary
    def node(cls,x,y):return lib.create_material_expression(mat,cls,x,y)
    def wire(a,b,pin,out=''):
        linked=lib.connect_material_expressions(a,out,b,pin)
        if not linked and pin in ('Input','Coordinates'):
            linked=lib.connect_material_expressions(a,out,b,'')
        if not linked:
            raise RuntimeError('SnowPBRDetail连线失败: '+b.get_name()+'.'+pin)
    def number(v,x,y):
        n=node(unreal.MaterialExpressionConstant,x,y);n.set_editor_property('r',v);return n
    def scalar(name,v,x,y):
        n=node(unreal.MaterialExpressionScalarParameter,x,y)
        n.set_editor_property('parameter_name',prefix+name);n.set_editor_property('default_value',v);return n
    def color(v,x,y):
        n=node(unreal.MaterialExpressionConstant3Vector,x,y)
        n.set_editor_property('constant',unreal.LinearColor(*v,1));return n
    def op(cls,a,b,x,y):
        n=node(cls,x,y);wire(a,n,'A');wire(b,n,'B');return n
    def lerp(a,b,alpha,x,y):
        n=node(unreal.MaterialExpressionLinearInterpolate,x,y)
        wire(a,n,'A');wire(b,n,'B');wire(alpha,n,'Alpha');return n
    def sample(name,coords,x,y):
        n=node(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
        n.set_editor_property('parameter_name',prefix+name)
        n.set_editor_property('texture',fallback)
        n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        wire(coords,n,'Coordinates');return n
    height=sample('HeightTexture',uv,-2100,4050)
    camera=node(unreal.MaterialExpressionCameraPositionWS,-2600,4350)
    dist=node(unreal.MaterialExpressionDistance,-2300,4350)
    wire(camera,dist,'A');wire(position,dist,'B')
    end=scalar('ParallaxEndCm',1200.,-2600,4550)
    span=scalar('ParallaxFadeSpanCm',800.,-2600,4700)
    fade=node(unreal.MaterialExpressionSaturate,-1740,4420)
    wire(op(unreal.MaterialExpressionDivide,op(unreal.MaterialExpressionSubtract,end,dist,-2080,4440),span,-1900,4440),fade,'Input')
    ratio=op(unreal.MaterialExpressionMultiply,scalar('ParallaxHeightRatio',.015,-2080,4700),fade,-1500,4440)
    bump=node(unreal.MaterialExpressionBumpOffset,-1200,4100)
    bump.set_editor_property('reference_plane',.5)
    wire(height,bump,'Height','R');wire(uv,bump,'Coordinate');wire(ratio,bump,'HeightRatioInput')
    wire(bump,albedo,'Coordinates');wire(bump,normal,'Coordinates')
    orm=sample('ORMTexture',bump,-950,4440)
    # 配套扫描粗糙度和AO产生连续高光/凹陷，不增加透明覆盖或随机闪点。
    rough=node(unreal.MaterialExpressionClamp,-480,4490)
    wire(orm,rough,'Input','G');rough.set_editor_property('min_default',.65);rough.set_editor_property('max_default',.96)
    ao=lerp(number(.80,-900,4800),number(1.,-900,4940),orm,-480,4800)
    wire(orm,ao,'Alpha','R')
    # 次表面色仅在实际积雪区贡献；未覆盖地表维持原材质数据。
    # 用户参考图给出线性RGB；保留BaseColor源像素，仅调节积雪的次表面散射色。
    tint=node(unreal.MaterialExpressionVectorParameter,-850,5150)
    tint.set_editor_property('parameter_name',prefix+' Subsurface Tint')
    tint.set_editor_property('default_value',unreal.LinearColor(.547744,.710151,1.,1.))
    sss=op(unreal.MaterialExpressionMultiply,albedo,tint,-480,5140)
    sss=op(unreal.MaterialExpressionMultiply,sss,coverage,-200,5140)
    opacity=lerp(number(1.,-900,5370),number(.72,-900,5500),coverage,-480,5360)
    return {'roughness':rough,'ao':ao,'subsurface':sss,'opacity':opacity,'height':height,'orm':orm}
