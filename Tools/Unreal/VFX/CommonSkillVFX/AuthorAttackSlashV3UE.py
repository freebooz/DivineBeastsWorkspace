# -*- coding: utf-8 -*-
"""凝锋V3的原生项目资源作者，经Monolith执行；复用既有导入工具，不引入运行播放器。"""
from pathlib import Path
import json,runpy
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V3/AttackSlash'
EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3';EVIDENCE.mkdir(parents=True,exist_ok=True)
helpers=runpy.run_path(str(Path(__file__).with_name('AuthorAttackSlashV2UE.py')),init_globals=dict(PHASE='library',SOURCE_DIR=str(SOURCE),ASSET_BASE=BASE,EVIDENCE_DIR=str(EVIDENCE),REIMPORT_OWN_SOURCE=globals().get('REIMPORT_OWN_SOURCE',False)))
LIB=unreal.MaterialEditingLibrary;TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
node=helpers['node'];wire=helpers['wire'];output=helpers['output'];scalar=helpers['scalar'];save=helpers['save']


def sources():
    helpers['import_sources']()
    t=unreal.load_asset(BASE+'/Textures/T_DBA_SlashV3_Energy_Color')
    t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_BC7)
    t.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
    t.set_editor_property('max_texture_size',2048)
    t.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);t.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP);save(t)
    print('V3_COLOR_SOURCE_RGB_ALPHA_PRESERVED',t.get_path_name())


def custom(m,names,code,kind):
    n=node(m,unreal.MaterialExpressionCustom);inputs=[]
    for name in names:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);inputs.append(i)
    n.set_editor_property('inputs',inputs);n.set_editor_property('code',code);n.set_editor_property('output_type',kind);return n


def materials():
    for kind in ('Body','FlowSheets','Filaments'):
        name='M_DBA_SlashV3_'+kind;m=unreal.load_asset(BASE+'/Materials/'+name)
        if m:LIB.delete_all_material_expressions(m)
        else:m=TOOLS.create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if kind=='Body' else unreal.BlendMode.BLEND_ADDITIVE)
        m.set_editor_property('two_sided',False);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        LIB.set_base_material_usage(m,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True)
        uv=node(m,unreal.MaterialExpressionTextureCoordinate);age=node(m,unreal.MaterialExpressionParticleRelativeTime);pc=node(m,unreal.MaterialExpressionParticleColor)
        normal=node(m,unreal.MaterialExpressionPixelNormalWS);view=node(m,unreal.MaterialExpressionCameraVectorWS)
        samples=[]
        for j in range(2 if kind!='Filaments' else 0):
            n=node(m,unreal.MaterialExpressionTextureSampleParameter2D);n.set_editor_property('parameter_name','EnergyArt');n.set_editor_property('texture',unreal.load_asset(BASE+'/Textures/T_DBA_SlashV3_Energy_Color'));n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
            uvcode=('return float2(UV.x-Clock*.16,.24+UV.y*.48+sin(UV.x*16-Clock*8)*.013);' if j==0 else 'return float2(UV.x*.91+Clock*.10,.16+UV.y*.50+sin(UV.x*11+Clock*6)*.02);')
            coords=custom(m,['UV','Clock'],uvcode,unreal.CustomMaterialOutputType.CMOT_FLOAT2);wire(uv,'',coords,'UV');wire(age,'',coords,'Clock');wire(coords,'',n,'UVs');samples.append(n)
        code='''float t=saturate(Clock);
float reveal=smoothstep(UV.x-.07,UV.x+.07,saturate(t/.25)*1.10);
float retreat=1-smoothstep(UV.x-.12,UV.x+.12,saturate((t-.70)/.30)*1.15);
float fade=smoothstep(0,.045,t)*smoothstep(0,.16,1-t);
float envelope=reveal*retreat*fade*PC.a;
'''
        if kind=='Filaments':
            code+='''float across=pow(saturate(1-abs(UV.y*2-1)),.5);
float travelling=.70+.30*sin(UV.x*21-t*13);
float3 rgb=lerp(float3(1,.17,.012),float3(1,.78,.28),across);
return float4(rgb*PC.rgb,across*travelling*envelope*.88);'''
            names=['UV','Clock','PC'];nodes=[(uv,'','UV'),(age,'','Clock'),(pc,'RGBA','PC')]
        else:
            code+='''float facing=.60+.40*abs(dot(normalize(N),normalize(V)));
float weight=Art.a+Aux.a*.55;
float3 energy=(Art.rgb*Art.a+Aux.rgb*Aux.a*.55)/max(.001,weight);
float hot=saturate(max(energy.g,energy.b));
float3 rgb=energy*(.80+hot*1.65)*facing;
float alpha=saturate(weight*.84);
'''
            code+=('return float4(rgb*PC.rgb,alpha*envelope*.86);' if kind=='Body' else 'return float4(rgb*PC.rgb,alpha*envelope*.48);')
            names=['UV','Clock','PC','Art','Aux','N','V'];nodes=[(uv,'','UV'),(age,'','Clock'),(pc,'RGBA','PC'),(samples[0],'RGBA','Art'),(samples[1],'RGBA','Aux'),(normal,'','N'),(view,'','V')]
        value=custom(m,names,code,unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        for src,pin,dest in nodes:wire(src,pin,value,dest)
        rgb=node(m,unreal.MaterialExpressionComponentMask)
        for p in ('r','g','b'):rgb.set_editor_property(p,True)
        rgb.set_editor_property('a',False);wire(value,'',rgb,'')
        alpha=node(m,unreal.MaterialExpressionComponentMask)
        for p in ('r','g','b'):alpha.set_editor_property(p,False)
        alpha.set_editor_property('a',True);wire(value,'',alpha,'')
        strength=scalar(m,'EmissionStrength',1.9 if kind=='Body' else 1.55 if kind=='FlowSheets' else 3.6)
        emiss=node(m,unreal.MaterialExpressionMultiply);wire(rgb,'',emiss,'A');wire(strength,'',emiss,'B');output(emiss,unreal.MaterialProperty.MP_EMISSIVE_COLOR);output(alpha,unreal.MaterialProperty.MP_OPACITY)
        vertexnormal=node(m,unreal.MaterialExpressionVertexNormalWS)
        amplitude=.8 if kind=='Body' else .10 if kind=='FlowSheets' else .015
        wobble=custom(m,['UV','Clock','N'],'return N*(sin(UV.x*17-Clock*9)*sin(UV.x*3.141593)*sin(UV.y*3.141593)*'+str(amplitude)+');',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        for s,p,d in [(uv,'','UV'),(age,'','Clock'),(vertexnormal,'','N')]:wire(s,p,wobble,d)
        output(wobble,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        LIB.layout_material_expressions(m);LIB.recompile_material(m);save(m)
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_AttackSlash_V3')
    for i,slot in enumerate(mesh.get_editor_property('static_materials')):
        key=next((k for k in ('Body','FlowSheets','Filaments') if k in str(slot.material_slot_name)),None);assert key
        mesh.set_material(i,unreal.load_asset(BASE+'/Materials/M_DBA_SlashV3_'+key))
    save(mesh);print('V3_THREE_LAYER_MATERIALS_SAVED')


def definition():
    name='DA_DBA_VFX_AttackSlashV3_Review';d=unreal.load_asset(BASE+'/Definitions/'+name)
    if not d:
        f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.GamePlatformVFXInstantDefinition);d=TOOLS.create_asset(name,BASE+'/Definitions',unreal.GamePlatformVFXInstantDefinition,f)
    ident=unreal.GamePlatformId(namespace='presentation.dba.common.attack.slash',name='review_v3',logical_version=1);d.set_editor_property('logical_id',ident)
    d.set_editor_property('niagara_system',unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V3'))
    for p,v in [('auto_destroy',True),('max_lifetime_seconds',3.),('allow_pooling',True),('require_fixed_bounds',True)]:d.set_editor_property(p,v)
    d.set_editor_property('default_parameters',unreal.GamePlatformVFXParameters(float_parameters={'User.Duration':.85,'User.Radius':230.,'User.Intensity':1.}))
    d.set_editor_property('parameter_schema',unreal.GamePlatformVFXParameterSchema(rules=[unreal.GamePlatformVFXParameterRule(name=n,type=unreal.GamePlatformVFXParameterType.FLOAT,min_value=lo,max_value=hi) for n,lo,hi in [('User.Duration',.25,2.),('User.Radius',100.,500.),('User.Intensity',.2,2.)]]));save(d)
    print('V3_REVIEW_DEFINITION_SAVED')


if PHASE=='import':sources()
elif PHASE=='materials':materials()
elif PHASE=='definition':definition()
else:raise ValueError(PHASE)
