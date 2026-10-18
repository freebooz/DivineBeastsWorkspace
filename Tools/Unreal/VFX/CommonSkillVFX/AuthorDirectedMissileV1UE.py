# -*- coding: utf-8 -*-
"""Monolith内的定向飞弹纹理/材质/平台Projectile定义作者，不含权威运动。"""
from pathlib import Path
import json,runpy
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile'
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile';EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1';EVIDENCE.mkdir(parents=True,exist_ok=True)
helpers=runpy.run_path(str(Path(__file__).with_name('AuthorAttackSlashV2UE.py')),init_globals=dict(PHASE='library',SOURCE_DIR=str(SOURCE),ASSET_BASE=BASE,EVIDENCE_DIR=str(EVIDENCE),IMPORT_VERTEX_COLORS=True,REIMPORT_OWN_SOURCE=globals().get('REIMPORT_OWN_SOURCE',False)))
node=helpers['node'];wire=helpers['wire'];output=helpers['output'];scalar=helpers['scalar'];save=helpers['save'];LIB=unreal.MaterialEditingLibrary
def custom(m,names,code,kind):
    n=node(m,unreal.MaterialExpressionCustom);inputs=[]
    for name in names:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    n.set_editor_property('inputs',inputs);n.set_editor_property('code',code);n.set_editor_property('output_type',kind);return n
def material(name,additive=False):
    path=BASE+'/Materials/M_DBA_MissileV1_'+name;m=unreal.load_asset(path)
    if m:LIB.delete_all_material_expressions(m)
    else:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.split('/')[-1],BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',False)
    LIB.set_base_material_usage(m,unreal.MaterialUsage.MATUSAGE_NIAGARA_RIBBONS if name=='Trail' else unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True);return m
def split_output(m,value,strength):
    rgb=node(m,unreal.MaterialExpressionComponentMask)
    for p in ('r','g','b'):rgb.set_editor_property(p,True)
    rgb.set_editor_property('a',False);wire(value,'',rgb,'')
    alpha=node(m,unreal.MaterialExpressionComponentMask)
    for p in ('r','g','b'):alpha.set_editor_property(p,False)
    alpha.set_editor_property('a',True);wire(value,'',alpha,'')
    mul=node(m,unreal.MaterialExpressionMultiply);wire(rgb,'',mul,'A');wire(scalar(m,'EmissionStrength',strength),'',mul,'B');output(mul,unreal.MaterialProperty.MP_EMISSIVE_COLOR);output(alpha,unreal.MaterialProperty.MP_OPACITY)
def materials():
    for kind in ('Core','Petals','Trail'):
        m=material(kind,kind=='Trail');uv=node(m,unreal.MaterialExpressionTextureCoordinate);age=node(m,unreal.MaterialExpressionParticleRelativeTime);pc=node(m,unreal.MaterialExpressionParticleColor)
        sample=node(m,unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','EnergyArt');sample.set_editor_property('texture',unreal.load_asset(BASE+'/Textures/T_DBA_MissileV1_'+('Trail' if kind=='Trail' else 'Head')+'_Color'));sample.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR);sample.set_editor_property('automatic_view_mip_bias',False);sample.set_editor_property('mip_value_mode',unreal.TextureMipValueMode.TMVM_MIP_BIAS);wire(scalar(m,'EnergyMipBias',1.8),'',sample,'Bias')
        coords=custom(m,['UV','Age'],'return float2(frac(UV.x-Age*.12),.16+UV.y*.68);',unreal.CustomMaterialOutputType.CMOT_FLOAT2);wire(uv,'',coords,'UV');wire(age,'',coords,'Age');wire(coords,'',sample,'UVs')
        code=''
        if kind=='Trail':code+='''float v=UV.y-.5;
float band=exp2(-v*v*70);
float spine=exp2(-v*v*1200);
float across=saturate(1-abs(v)/.48);
float3 rgb=lerp(float3(.70,.020,.0005),float3(1.8,.16+.14*PC.g,.008),across);
rgb=lerp(rgb,float3(5,3,.70),spine*.18)*(.99+.01*Art.g);
return float4(rgb,band*.90*PC.a*smoothstep(0,.15,UV.x));'''
        else:
            code+='''float envelope=smoothstep(0,.045,Age)*smoothstep(0,.12,1-Age);
float facing=abs(dot(normalize(N),normalize(V)));
float flow=sin(UV.x*14-Age*7+UV.y*4+sin(UV.x*7+UV.y*5-Age*4)*1.2);
float vein=smoothstep(.82,.99,flow);
float detail=.97+.03*Art.g;
float3 rgb;float alpha;
'''
            if kind=='Core':code+='''float hot=smoothstep(.78,.995,UV.x);
float3 body=lerp(float3(.30,.018,.001),float3(.70,.12,.003),pow(facing,.7));
body=lerp(body,float3(5,3.5,1),hot)+vein*float3(.18,.055,.001);
rgb=lerp(body,float3(12,6,1.5),saturate(Layer))*detail;
// 保留体积的内核比薄瓣更实，局部辉光按视角柔化，不能整片叠加烧白。
alpha=lerp(.80,pow(facing,4.5)*.50,saturate(Layer))*PC.a;
'''
            else:code+='''float rim=pow(saturate((UV.y-.58)/.42),6);
float shade=.55+.45*pow(facing,.7);
// 对未折返坐标求导，避免frac边界造成白边破碎与闪点。
float facetCoord=UV.x*2.2-abs(UV.y-.5)*1.5+Layer*.22+sin(UV.x*7+UV.y*3)*.14;
float facet=frac(facetCoord);
float aa=max(fwidth(facetCoord),.006);
float cut=smoothstep(.08-aa,.12+aa,facet);
float seam=smoothstep(.10-aa,.13+aa,facet)*(1-smoothstep(.16-aa,.19+aa,facet));
rgb=(float3(.42,.05,.001)*(1-.3*Layer)+float3(.48,.085,.002)*shade)*detail;
rgb*=lerp(.35,1,cut);
// 金边/流动细锋局部高亮，暗橙瓣面负责体积与明暗对比。
rgb=lerp(rgb,float3(4.6,2.5,.48),max(rim*.84,seam*.80));
rgb+=vein*float3(.25,.09,.003);
alpha=(.30+.40*pow(facing,.6)+.16*rim)*PC.a*smoothstep(0,.12,facing);
'''
            code+='return float4(rgb*PC.rgb,alpha*envelope);'
        names=['UV','Age','PC','Art'];links=[(uv,'','UV'),(age,'','Age'),(pc,'RGBA','PC'),(sample,'RGBA','Art')]
        if kind!='Trail':
            normal=node(m,unreal.MaterialExpressionPixelNormalWS);view=node(m,unreal.MaterialExpressionCameraVectorWS);layer=node(m,unreal.MaterialExpressionVertexColor);names+=['N','V','Layer'];links +=[(normal,'','N'),(view,'','V'),(layer,'R','Layer')]
        value=custom(m,names,code,unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        for s,p,d in links:wire(s,p,value,d)
        split_output(m,value,2.3 if kind=='Core' else 1.75 if kind=='Petals' else 3.0);LIB.layout_material_expressions(m);LIB.recompile_material(m);save(m)
    m=material('AirWave');m.set_editor_property('refraction_method',unreal.RefractionMode.RM_2D_OFFSET);uv=node(m,unreal.MaterialExpressionTextureCoordinate);age=node(m,unreal.MaterialExpressionParticleRelativeTime);strength=scalar(m,'AirWaveStrength',.35)
    offset=custom(m,['UV','Age','Strength'],'''float mask=sin(UV.x*3.141593)*sin(UV.y*3.141593)*smoothstep(0,.06,Age)*smoothstep(0,.16,1-Age);
return float2(sin(UV.x*18-Age*11),cos(UV.x*13+UV.y*8-Age*9))*(Strength*.003*mask);''',unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    for s,p,d in [(uv,'','UV'),(age,'','Age'),(strength,'','Strength')]:wire(s,p,offset,d)
    output(offset,unreal.MaterialProperty.MP_REFRACTION);zero=node(m,unreal.MaterialExpressionConstant);zero.set_editor_property('r',0.);output(zero,unreal.MaterialProperty.MP_OPACITY);output(zero,unreal.MaterialProperty.MP_EMISSIVE_COLOR);LIB.recompile_material(m);save(m)
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_DirectedMissile_V1')
    for i,s in enumerate(mesh.get_editor_property('static_materials')):
        key=next(k for k in ('Core','Petals','AirWave') if k in str(s.material_slot_name));mesh.set_material(i,unreal.load_asset(BASE+'/Materials/M_DBA_MissileV1_'+key))
    save(mesh);print('MISSILE_FOUR_MATERIALS_SAVED')
def definitions():
    name='DA_DBA_VFX_DirectedMissileV1_Review';d=unreal.load_asset(BASE+'/Definitions/'+name)
    if not d:
        f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.GamePlatformVFXProjectileDefinition);d=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Definitions',unreal.GamePlatformVFXProjectileDefinition,f)
    d.set_editor_property('logical_id',unreal.GamePlatformId(namespace='presentation.dba.common.attack.directedmissile',name='review_v1',logical_version=1));d.set_editor_property('niagara_system',unreal.load_asset(BASE+'/Niagara/NS_DBA_DirectedMissile_V1'));d.set_editor_property('effect_type',unreal.load_asset(BASE+'/Scalability/NET_DBA_Review_CombatProjectile'))
    for p,v in [('auto_destroy',True),('allow_pooling',True),('require_fixed_bounds',True),('max_lifetime_seconds',3.)]:d.set_editor_property(p,v)
    values={'User.HeadLifetime':.85,'User.TrailLifetime':1.,'User.HeadScale':1.,'User.TrailWidth':26.,'User.Intensity':1.}
    limits={'User.HeadLifetime':(.25,1.),'User.TrailLifetime':(.2,1.2),'User.HeadScale':(.5,1.5),'User.TrailWidth':(4.,30.),'User.Intensity':(.2,2.)}
    d.set_editor_property('default_parameters',unreal.GamePlatformVFXParameters(float_parameters=values,vector_parameters={'User.Direction':unreal.Vector(1,0,0)}))
    rules=[unreal.GamePlatformVFXParameterRule(name=n,type=unreal.GamePlatformVFXParameterType.FLOAT,min_value=lo,max_value=hi) for n,(lo,hi) in limits.items()];rules.append(unreal.GamePlatformVFXParameterRule(name='User.Direction',type=unreal.GamePlatformVFXParameterType.VECTOR,min_value=-1,max_value=1));d.set_editor_property('parameter_schema',unreal.GamePlatformVFXParameterSchema(rules=rules));save(d);print('MISSILE_PROJECTILE_DEFINITION_SAVED')
if PHASE=='import':
    helpers['import_sources']()
    for kind in ('Head','Trail'):
        t=unreal.load_asset(BASE+'/Textures/T_DBA_MissileV1_'+kind+'_Color');t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7);t.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO);t.set_editor_property('max_texture_size',2048);t.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);save(t)
elif PHASE=='materials':materials()
elif PHASE=='definition':definitions()
else:raise ValueError(PHASE)
