# -*- coding: utf-8 -*-
"""SSLM V2返工：原生导入新几何，制作独立连续火焰与多面冰晶材质。
仅写OrbProjectile/Review/V2；不复制V1粗糙母材质，不改正式Gameplay；技术检查不替代人工视觉。
"""
from pathlib import Path
import unreal,json,traceback,hashlib
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SRC=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/OrbProjectile/Review/V2'
DEST='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2'
OUT=ROOT/'Saved/Validation/OrbProjectileV2'
LIB=unreal.MaterialEditingLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
report={'status':'begin','created':[],'manualAccepted':False,'V1Modified':False,'errors':[]}

# 连续三维低频值噪声，作用于真实球面坐标，不依赖错误切片的旧图集。
NOISE=r'''
struct NoiseField {
 float hash(float3 p){return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453);}
 float noise(float3 p){
  float3 i=floor(p);float3 f=frac(p);f=f*f*(3-2*f);
  return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
 }
 float fbm(float3 p){return .58*noise(p)+.28*noise(p*2.03+7.1)+.14*noise(p*4.07+13.4);}
};
NoiseField nf;
'''
FIRE=NOISE+r'''
float ph=UV.x*6.2831853;float th=UV.y*3.1415927;
float3 p=float3(cos(ph)*sin(th),sin(ph)*sin(th),cos(th));
float3 q=p*3.8+float3(.12*T,.20*T,-.65*T);
float warp=nf.fbm(q*.75+T*.16);
float a=nf.fbm(q+float3(warp*1.6,warp*.8,warp*.2));
float stream=smoothstep(.33,.74,a);
float center=pow(saturate(1.0-Edge),.6);
float3 ember=float3(1.5,.105,.009);
float3 gold=float3(4.6,1.35,.07);
float3 whitehot=float3(8.5,5.1,1.4);
float3 c=lerp(ember,gold,stream);
c=lerp(c,whitehot,smoothstep(.62,.83,a)*(.35+.65*center));
c*=.62+.38*center;
return float4(c,1);
'''
FLAME=NOISE+r'''
float u=UV.x*2-1;float v=UV.y;
float bend=.15*sin(v*7-T*3)+.10*sin(v*13+T*1.8);
float width=lerp(.74,.06,smoothstep(.12,.98,1-v));
float3 q=float3((u+bend)*3.3,v*4.6-T*1.7,T*.19);
float n=nf.fbm(q);
float edge=1-smoothstep(width*.52,width,abs(u+bend));
float mask=edge*smoothstep(0,.12,v)*(1-smoothstep(.92,1,v));
mask*=smoothstep(.20,.67,n+.12*v);
float heat=saturate(.52*n+.48*v);
float3 c=lerp(float3(1.9,.15,.008),float3(6,2.2,.22),smoothstep(.28,.85,heat));
return float4(c,saturate(mask*.82));
'''
ICE=NOISE+r'''
float ph=UV.x*6.2831853;float th=UV.y*3.1415927;
float3 p=float3(cos(ph)*sin(th),sin(ph)*sin(th),cos(th));
float n=nf.fbm(p*2.9);
float m=nf.fbm(p*5.1+11.2);
float thread=1-smoothstep(.012,.045,abs(n-.54));
thread*=smoothstep(.52,.76,m);
float frost=smoothstep(.58,.77,n)*.38+thread*.10;
float3 c=lerp(float3(.025,.18,.28),float3(.23,.56,.68),n*.72+frost);
return float4(c,saturate(.82+Edge*.16));
'''
MIST=NOISE+r'''
float2 uv=(UV-.5)*2;
float n=nf.fbm(float3(uv*2.4+float2(T*.12,-T*.21),T*.15));
float r=length(uv*float2(1,.86));
float a=(1-smoothstep(.26,.98,r))*smoothstep(.25,.75,n)*.32;
return float4(.24,.64,.92,a);
'''
TRAIL=NOISE+r'''
float cross=abs(UV.y*2-1);
float n=nf.fbm(float3(UV.x*7-T*1.4,UV.y*2.6,T*.12));
float a=pow(saturate(1-cross*cross),1.8)*(.48+.52*n);
float3 fire=lerp(float3(2.2,.23,.015),float3(5.4,2.0,.16),n);
float3 ice=lerp(float3(.14,.42,.72),float3(.42,1.25,2.0),n);
return float4(lerp(fire,ice,saturate(Flavor)),a);
'''

def node(m,cls,x=0,y=0):return LIB.create_material_expression(m,cls,x,y)
def connect(a,out,b,name):
    if not LIB.connect_material_expressions(a,out,b,name):raise RuntimeError('材质连接失败 '+b.get_name()+'/'+name)
def output(a,prop,out=''):
    if not LIB.connect_material_property(a,out,prop):raise RuntimeError('材质输出连接失败 '+str(prop))
def scalar(m,name,value):
    n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(m,name,value):
    n=node(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));return n
def mask(m,source,channels):
    n=node(m,unreal.MaterialExpressionComponentMask)
    for c in 'rgba':n.set_editor_property(c,c in channels)
    connect(source,'',n,'');return n
def multiply(m,a,b,aout='',bout=''):
    n=node(m,unreal.MaterialExpressionMultiply);connect(a,aout,n,'A');connect(b,bout,n,'B');return n
def custom(m,code,inputs):
    c=node(m,unreal.MaterialExpressionCustom)
    c.set_editor_property('code',code)
    c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    defs=[]
    for name in inputs:
        v=unreal.CustomInput();v.set_editor_property('input_name',name);defs.append(v)
    c.set_editor_property('inputs',defs)
    for name,value in inputs.items():connect(value,'',c,name)
    return c

def save(a):
    if not unreal.EditorAssetLibrary.save_loaded_asset(a,False):raise RuntimeError('原生保存失败 '+a.get_path_name())
    report['created'].append(a.get_path_name())

def make_material(name,code,kind):
    path=DEST+'/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        # 只恢复本轮记录过的未完成核心图；不能重写任何已保存材质。
        previous=json.loads((OUT/'Surfaces.json').read_text(encoding='utf8'))
        assert name=='M_DBA_FireCoreFlow_V2' and previous['status']=='failed' and len(previous['created'])==2
        m=unreal.load_asset(path)
        assert isinstance(m,unreal.Material)
        LIB.delete_all_material_expressions(m)
    else:
        m=TOOLS.create_asset(name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided',kind in ('flame','mist','trail'))
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE if kind=='core' else unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT if kind=='ice' else unreal.MaterialShadingModel.MSM_UNLIT)
    if kind=='ice':m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    for usage in (unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,unreal.MaterialUsage.MATUSAGE_NIAGARA_RIBBONS):LIB.set_base_material_usage(m,usage,True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate,-700,0)
    tm=node(m,unreal.MaterialExpressionTime,-700,180)
    fr=node(m,unreal.MaterialExpressionFresnel,-700,340);fr.set_editor_property('exponent',3.0);fr.set_editor_property('base_reflect_fraction',.02)
    entries={'UV':uv,'T':tm,'Edge':fr}
    if kind=='trail':entries['Flavor']=scalar(m,'Flavor',0)
    expr=custom(m,code,entries)
    rgb=mask(m,expr,'rgb');alpha=mask(m,expr,'a')
    pc=node(m,unreal.MaterialExpressionParticleColor)
    if kind=='ice':
        output(rgb,unreal.MaterialProperty.MP_BASE_COLOR)
        output(scalar(m,'Roughness',.17),unreal.MaterialProperty.MP_ROUGHNESS)
        output(scalar(m,'Specular',.72),unreal.MaterialProperty.MP_SPECULAR)
        glow=multiply(m,rgb,scalar(m,'InteriorGlow',.36))
        output(glow,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        output(alpha,unreal.MaterialProperty.MP_OPACITY)
    else:
        strength=scalar(m,'Emission',1.0 if kind=='core' else .85)
        emitted=multiply(m,rgb,strength)
        output(emitted,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if kind!='core':
            opacity=multiply(m,alpha,pc,'','A')
            output(opacity,unreal.MaterialProperty.MP_OPACITY)
    LIB.layout_material_expressions(m)
    LIB.recompile_material(m)
    save(m)
    return m

try:
    assert '-OrbV2ReworkOwned' in unreal.SystemLibrary.get_command_line()
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
    for name in ('SM_DBA_FireCore_V2','SM_DBA_IceCrystal_V2'):
        path=DEST+'/Meshes/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            previous=json.loads((OUT/'Surfaces.json').read_text(encoding='utf8'))
            assert any(path in p for p in previous['created']),'该网格不属于本轮失败恢复'
            obj=unreal.load_asset(path)
            assert isinstance(obj,unreal.StaticMesh)
            report['created'].append(obj.get_path_name())
            continue
        t=unreal.AssetImportTask();t.set_editor_property('filename',str(SRC/(name+'.obj')))
        t.set_editor_property('destination_path',DEST+'/Meshes');t.set_editor_property('destination_name',name)
        t.set_editor_property('automated',True);t.set_editor_property('save',False);t.set_editor_property('replace_existing',False)
        options=unreal.FbxImportUI();options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False);options.set_editor_property('import_as_skeletal',False)
        data=options.get_editor_property('static_mesh_import_data')
        data.set_editor_property('auto_generate_collision',False)
        data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
        t.set_editor_property('options',options)
        TOOLS.import_asset_tasks([t]);obj=unreal.load_asset(path)
        if not isinstance(obj,unreal.StaticMesh):raise RuntimeError('网格原生导入失败 '+name)
        save(obj)
    make_material('M_DBA_FireCoreFlow_V2',FIRE,'core')
    make_material('M_DBA_FireVeil_V2',FLAME,'flame')
    make_material('M_DBA_IceCrystal_V2',ICE,'ice')
    make_material('M_DBA_ColdMist_V2',MIST,'mist')
    trail=make_material('M_DBA_OrbTrail_V2',TRAIL,'trail')
    for name,value in [('MI_DBA_FireTrail_V2',0.0),('MI_DBA_IceTrail_V2',1.0)]:
        path=DEST+'/Materials/'+name
        if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('已有审核材质实例 '+path)
        a=TOOLS.create_asset(name,DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(a,trail);LIB.set_material_instance_scalar_parameter_value(a,'Flavor',value);LIB.update_material_instance(a);save(a)
    report['status']='native_surfaces_created'
except Exception:
    report['status']='failed';report['errors'].append(traceback.format_exc())
    print('ORBV2_SURFACES_ERROR',report['errors'][-1],flush=True)
finally:
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'Surfaces.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('ORBV2_SURFACES_RESULT',report['status'],len(report['created']),flush=True)
