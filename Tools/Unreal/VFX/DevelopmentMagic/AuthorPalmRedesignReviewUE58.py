# -*- coding: utf-8 -*-
"""SSLM掌心效果图原生作者，仅由Monolith在UE5.8主线程执行。
资源归项目表现内容包的独立审核目录；不修改原V3、正式Catalog或权威时序。
拥有者清单和原稿SHA限制可恢复写入范围，失败记录留存供审计。
"""
from pathlib import Path
import unreal as u,json,hashlib
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
OUT=ROOT/'Saved/Validation/PalmRedesignReview20261026'
SRC=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/PalmRedesign'
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign'
LIB='/DBAPresentationPack_Core/VFX/Library/Game/FireMage'
EL=u.EditorAssetLibrary;ML=u.MaterialEditingLibrary;AT=u.AssetToolsHelpers.get_asset_tools()

def record(name,value):
    """只写审核证据，不当作引擎资产或验收结论。"""
    OUT.mkdir(parents=True,exist_ok=True);(OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2,default=str),encoding='utf8')

def protected():
    """每个写入阶段验证原稿字节不变。"""
    for r in json.loads((OUT/'Protected.before.json').read_text()):
        assert hashlib.sha256((ROOT/r['path']).read_bytes()).hexdigest()==r['sha256'],r['path']

def claim(path):
    """只认领不存在的审核目标；存在且不在本次清单的目标拒绝覆盖。"""
    assert path.startswith(BASE+'/')
    f=OUT/'OwnedAssets.json';owned=json.loads(f.read_text()) if f.exists() else []
    if EL.does_asset_exist(path):assert path in owned,'非本轮资源 '+path
    elif path not in owned:owned.append(path);record('OwnedAssets.json',owned)

def save(a):
    assert a.get_path_name().startswith(BASE+'/');assert EL.save_loaded_asset(a,only_if_is_dirty=False);return a

def make(path,cls,factory):
    claim(path)
    return u.load_asset(path) if EL.does_asset_exist(path) else AT.create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],cls,factory)

def import_sources(reimport_meshes=False):
    """通过引擎真实导入OBJ及纹理，记录真实包围盒和贴图导入设置。"""
    protected();result=[]
    source=json.loads((SRC/'SourceManifest.json').read_text())
    for kind in ['meshes','textures']:
        for row in source[kind]:
            file=SRC/row['file'];name=file.stem;path=BASE+('/Meshes/' if kind=='meshes' else '/Textures/')+name
            claim(path)
            if not EL.does_asset_exist(path) or (reimport_meshes and kind=='meshes'):
                t=u.AssetImportTask();t.filename=str(file);t.destination_path=path.rsplit('/',1)[0];t.destination_name=name
                t.automated=True;t.save=False;t.replace_existing=reimport_meshes
                if kind=='meshes':
                    op=u.FbxImportUI();op.import_materials=False;op.import_textures=False;op.import_as_skeletal=False
                    d=op.static_mesh_import_data;d.auto_generate_collision=False;d.normal_import_method=u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
                    t.options=op
                AT.import_asset_tasks([t])
            a=u.load_asset(path);assert a,path
            if kind=='textures':
                a.set_editor_property('srgb',False)
                a.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_NORMALMAP if name.endswith('_Normal') else u.TextureCompressionSettings.TC_MASKS)
                result.append({'asset':path,'size':[a.blueprint_get_size_x(),a.blueprint_get_size_y()],'sRGB':False})
            else:result.append({'asset':path,'bounds':str(a.get_bounds()),'trianglesSource':row['triangles']})
            save(a)
    protected();record('Imported.json',result);print('PALM_IMPORT',len(result))

def expr(m,cls,**props):
    a=ML.create_material_expression(m,cls,0,0)
    for k,v in props.items():a.set_editor_property(k,v)
    return a

def scalar(m,n,v):return expr(m,u.MaterialExpressionScalarParameter,parameter_name=n,default_value=float(v))

def custom(m,code,inputs,typ=None):
    a=expr(m,u.MaterialExpressionCustom,code=code,output_type=typ or u.CustomMaterialOutputType.CMOT_FLOAT4)
    pins=[]
    for k in inputs:
        p=u.CustomInput();p.set_editor_property('input_name',k);pins.append(p)
    a.set_editor_property('inputs',pins)
    for k,value in inputs.items():
        node,pin=value if isinstance(value,tuple) else (value,'')
        assert ML.connect_material_expressions(node,pin,a,k),k
    return a

def output(m,node):
    rgb=expr(m,u.MaterialExpressionComponentMask,r=True,g=True,b=True,a=False)
    alpha=expr(m,u.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True)
    assert ML.connect_material_expressions(node,'',rgb,'');assert ML.connect_material_expressions(node,'',alpha,'')
    assert ML.connect_material_property(rgb,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert ML.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
    return rgb,alpha

def instance(n,parent,values):
    a=make(BASE+'/Materials/'+n,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    ML.set_material_instance_parent(a,parent)
    for k,v in values.items():
        ML.set_material_instance_scalar_parameter_value(a,k,float(v))
        assert abs(ML.get_material_instance_scalar_parameter_value(a,k)-v)<1e-4,(n,k)
    return save(a)

def material_base(n,lit=False):
    a=make(BASE+'/Materials/'+n,u.Material,u.MaterialFactoryNew());ML.delete_all_material_expressions(a)
    a.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT if lit else u.BlendMode.BLEND_ALPHA_COMPOSITE)
    a.set_editor_property('shading_model',u.MaterialShadingModel.MSM_DEFAULT_LIT if lit else u.MaterialShadingModel.MSM_UNLIT)
    a.set_editor_property('two_sided',True);a.set_editor_property('used_with_niagara_mesh_particles',True);a.set_editor_property('used_with_niagara_sprites',True)
    return a

def materials():
    """真实创建Mesh火焰/冰晶及冷雾，不覆盖任何共用母材质；所有Alpha仅乘一次包络。"""
    protected();created=[]
    m=material_base('M_DBA_PalmFireCurl_Review')
    uv=expr(m,u.MaterialExpressionTextureCoordinate);pc=expr(m,u.MaterialExpressionParticleColor);tm=expr(m,u.MaterialExpressionDynamicParameter)
    tex=expr(m,u.MaterialExpressionTextureObjectParameter,parameter_name='FlameAtlas',texture=u.load_asset(LIB+'/Textures/T_PY_FlameFlipbook'))
    cv=expr(m,u.MaterialExpressionCameraVectorWS);nv=expr(m,u.MaterialExpressionVertexNormalWS)
    inputs={'UV':uv,'D':(tm,'RGBA'),'PC':pc,'PA':(pc,'A'),'Atlas':tex,'V':cv,'N':nv}
    for k,v in {'Energy':3.5,'Density':.85,'CoreMode':0,'FlowSpeed':.35}.items():inputs[k]=scalar(m,k,v)
    code=r'''
float t=D.x;float seed=D.y;float2 uv=UV;
float frame=frac(t*FlowSpeed*2+seed)*16;float i=floor(frame),j=fmod(i+1,16);
float2 w=float2(.065*sin(uv.y*12-t*2+seed*6),.035*sin(uv.x*8+t));
float2 q=clamp(uv+w,.025,.975);
float4 f=lerp(Texture2DSample(Atlas,AtlasSampler,(q+float2(fmod(i,4),floor(i/4)))/4),Texture2DSample(Atlas,AtlasSampler,(q+float2(fmod(j,4),floor(j/4)))/4),frac(frame));
float lum=dot(f.rgb,float3(.30,.55,.15));
float facing=saturate(abs(dot(normalize(V),normalize(N))));
float edge=smoothstep(0,.07,uv.y)*smoothstep(1,.95,uv.y)*pow(facing,.35);
float vein=pow(saturate(lum*2.2),1.5);
float3 c=lerp(float3(.75,.026,.002),float3(1,.34,.018),saturate(lum*1.7));
c=lerp(c,float3(1,.82,.38),pow(saturate(lum*1.75),3)*.8);
float a=saturate(pow(f.a,.65)*edge*Density*PA);
c*=Energy*(.38+.9*vein)*PC;
if(CoreMode>.5){float bands=.5+.5*sin(N.z*18+sin(N.x*13+t*8)+N.y*9-t*6);c=lerp(float3(1,.28,.012),float3(1,.85,.43),bands)*Energy*PC;a=PA*Density*pow(facing,1.2);}
return float4(c*a,a);
'''
    node=custom(m,code,inputs);output(m,node)
    normal=expr(m,u.MaterialExpressionVertexNormalWS)
    wpo=custom(m,'return N*(.5*sin(UV.y*14+D.x*3+D.y*6)+.3*sin(UV.x*17-D.x*2))*(1-CoreMode);',{'N':normal,'UV':uv,'D':(tm,'RGBA'),'CoreMode':inputs['CoreMode']},u.CustomMaterialOutputType.CMOT_FLOAT3)
    ML.connect_material_property(wpo,'',u.MaterialProperty.MP_WORLD_POSITION_OFFSET);ML.recompile_material(m);save(m);created.append(m)
    for n,vals in [('Core',{'Energy':9,'Density':.85,'CoreMode':1}),('Curl',{'Energy':3.4,'Density':.95,'CoreMode':0}),('Detail',{'Energy':2,'Density':.55,'CoreMode':0})]:created.append(instance('MI_DBA_PalmFire_'+n+'_Review',m,vals))
    m=material_base('M_DBA_PalmIceCrystal_Review',True)
    m.set_editor_property('translucency_lighting_mode',u.TranslucencyLightingMode.TLM_SURFACE)
    uv=expr(m,u.MaterialExpressionTextureCoordinate);pc=expr(m,u.MaterialExpressionParticleColor);tm=expr(m,u.MaterialExpressionDynamicParameter)
    cv=expr(m,u.MaterialExpressionCameraVectorWS);nv=expr(m,u.MaterialExpressionVertexNormalWS)
    refl=expr(m,u.MaterialExpressionReflectionVectorWS)
    env=expr(m,u.MaterialExpressionTextureSampleParameterCube,parameter_name='IceReflection',texture=u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    assert ML.connect_material_expressions(refl,'',env,'UVs')
    tex=expr(m,u.MaterialExpressionTextureObjectParameter,parameter_name='IceSurface',texture=u.load_asset(BASE+'/Textures/T_DBA_PalmIce_Surface'))
    inp={'UV':uv,'D':(tm,'RGBA'),'PC':pc,'PA':(pc,'A'),'Surface':tex,'V':cv,'N':nv,'Env':(env,'RGB')}
    for k,v in {'EmissiveStrength':1.8,'OpacityScale':.78,'Roughness':.14,'CoreMode':1}.items():inp[k]=scalar(m,k,v)
    code=r'''
float4 s=Texture2DSample(Surface,SurfaceSampler,UV);
float f=pow(1-saturate(abs(dot(normalize(V),normalize(N)))),3);
float stripe=.5+.5*sin(UV.x*19+sin(UV.y*13)+D.x*.6);
float3 normal=normalize(N);
float facets=pow(abs(dot(normal,normalize(float3(.6,-.4,1)))),6);
float facets2=pow(abs(dot(normal,normalize(float3(-.7,.8,.3)))),10);
float3 c=lerp(float3(.025,.085,.24),float3(.6,.78,1),saturate(s.g*.08+f*.12+facets*.65+facets2*.9));
float a=saturate((.42+.22*s.g+.2*f+.35*facets)*OpacityScale*PA);
float3 reflection=Env/(1+Env)*float3(.55,.8,1.0);
return float4((c*(.55+f*.25+facets*1.6+facets2*1.8)+reflection*(.5+f*.8)+s.a*float3(.4,.65,1)*.2)*PC,a);
'''
    node=custom(m,code,inp);rgb,alpha=output(m,node)
    ML.connect_material_property(rgb,'',u.MaterialProperty.MP_BASE_COLOR)
    glow=custom(m,'return C*(.6+.4*CoreMode)*Strength;',{'C':rgb,'CoreMode':inp['CoreMode'],'Strength':inp['EmissiveStrength']},u.CustomMaterialOutputType.CMOT_FLOAT3)
    ML.connect_material_property(glow,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    ML.connect_material_property(inp['Roughness'],'',u.MaterialProperty.MP_ROUGHNESS)
    spec=scalar(m,'Specular',.5);ML.connect_material_property(spec,'',u.MaterialProperty.MP_SPECULAR)
    norm=expr(m,u.MaterialExpressionTextureSampleParameter2D,parameter_name='IceNormal',texture=u.load_asset(BASE+'/Textures/T_DBA_PalmIce_Normal'),sampler_type=u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    ML.connect_material_property(norm,'RGB',u.MaterialProperty.MP_NORMAL)
    ML.recompile_material(m);save(m);created.append(m)
    created.append(instance('MI_DBA_PalmIce_Core_Review',m,{'EmissiveStrength':2.3,'OpacityScale':.85,'Roughness':.14,'CoreMode':1}))
    created.append(instance('MI_DBA_PalmIce_Petal_Review',m,{'EmissiveStrength':2.8,'OpacityScale':.8,'Roughness':.16,'CoreMode':0}))
    m=material_base('M_DBA_PalmMist_Review')
    uv=expr(m,u.MaterialExpressionTextureCoordinate);pc=expr(m,u.MaterialExpressionParticleColor);tm=expr(m,u.MaterialExpressionDynamicParameter)
    tex=expr(m,u.MaterialExpressionTextureObjectParameter,parameter_name='MistAtlas',texture=u.load_asset(BASE+'/Textures/T_DBA_PalmMist_Flipbook'))
    node=custom(m,r'''
float frame=frac(D.x*.75+D.y)*16;float i=floor(frame),j=fmod(i+1,16);float2 q=clamp(UV,.025,.975);
float4 f=lerp(Texture2DSample(Atlas,AtlasSampler,(q+float2(fmod(i,4),floor(i/4)))/4),Texture2DSample(Atlas,AtlasSampler,(q+float2(fmod(j,4),floor(j/4)))/4),frac(frame));
float a=saturate(f.a*PA*OpacityScale);float3 c=float3(.46,.7,1)*(.65+f.r*.25)*PC*EmissiveStrength;
return float4(c*a,a);
''',{'UV':uv,'D':(tm,'RGBA'),'PC':pc,'PA':(pc,'A'),'Atlas':tex,'OpacityScale':scalar(m,'OpacityScale',.32),'EmissiveStrength':scalar(m,'EmissiveStrength',1.6)})
    output(m,node);ML.recompile_material(m);save(m);created.append(m)
    created.append(instance('MI_DBA_PalmIce_Mist_Review',m,{'OpacityScale':.72,'EmissiveStrength':2.8}))
    # 原热浪母材质复制到本审核目录，Usage修改只落本副本，原始公共材质不变。
    source=u.load_asset('/DBAPresentationPack_Core/VFX/Library/00_Core/Materials/Fire/MI_VFX_Fire_Heat01')
    parent=source.get_editor_property('parent')
    while isinstance(parent,u.MaterialInstanceConstant):parent=parent.get_editor_property('parent')
    p=BASE+'/Materials/M_DBA_PalmAirRipple_Review';claim(p)
    ripple=u.load_asset(p) if EL.does_asset_exist(p) else EL.duplicate_asset(parent.get_path_name().split('.')[0],p)
    assert ripple
    # 旧热浪的有色吸收在球面形成暗壳；本审核副本改为零着色的局部折射。
    ML.delete_all_material_expressions(ripple);ripple.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
    ripple.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT);ripple.set_editor_property('two_sided',False)
    ripple.set_editor_property('refraction_method',u.RefractionMode.RM_INDEX_OF_REFRACTION)
    ripple.set_editor_property('used_with_niagara_mesh_particles',True)
    pc=expr(ripple,u.MaterialExpressionParticleColor);tm=expr(ripple,u.MaterialExpressionDynamicParameter);uv=expr(ripple,u.MaterialExpressionTextureCoordinate)
    zero=scalar(ripple,'P_Opacity',0);ML.connect_material_property(zero,'',u.MaterialProperty.MP_OPACITY)
    ref=custom(ripple,'return 1+Strength*A*sin(UV.y*24+D.x*9)*sin(UV.x*18-D.x*6);',{'Strength':scalar(ripple,'P_Intensity',.008),'A':(pc,'A'),'UV':uv,'D':(tm,'RGBA')},u.CustomMaterialOutputType.CMOT_FLOAT1)
    ML.connect_material_property(ref,'',u.MaterialProperty.MP_REFRACTION)
    ML.recompile_material(ripple);save(ripple);created.append(ripple)
    for typ,energy in [('Fire',.008),('Ice',.003)]:created.append(instance('MI_DBA_Palm'+typ+'_Ripple_Review',ripple,{'P_Intensity':energy,'P_Opacity':0}))
    protected();record('Materials.json',[a.get_path_name() for a in created]);print('PALM_MATERIALS',len(created))
