# -*- coding: utf-8 -*-
"""攻击弧刃V2的UE原生作者，必须经Monolith在已核对的编辑器中执行。

仅导入一个三材质网格与三张职责纹理，制作三份专用材质及一个Instant Definition。
不改旧原型、正式技能或全局光照/画质，不创建播放器。
"""
from pathlib import Path
import json
import os
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SOURCE=Path(globals().get('SOURCE_DIR',ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V2/AttackSlash'))
BASE=globals().get('ASSET_BASE','/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash')
EVIDENCE=Path(globals().get('EVIDENCE_DIR',ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2'));EVIDENCE.mkdir(parents=True,exist_ok=True)
assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
TOOLS=unreal.AssetToolsHelpers.get_asset_tools();LIB=unreal.MaterialEditingLibrary


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('原生保存失败：'+asset.get_path_name())


def import_sources():
    report=[]
    for source in sorted(SOURCE.glob('SM_DBA_*.fbx'))+sorted(SOURCE.glob('T_DBA_*.png')):
        texture=source.suffix=='.png';folder=BASE+('/Textures' if texture else '/Meshes');path=folder+'/'+source.stem
        asset=unreal.load_asset(path)
        if asset is None or globals().get('REIMPORT_OWN_SOURCE',False):
            task=unreal.AssetImportTask();task.set_editor_property('filename',str(source));task.set_editor_property('destination_path',folder)
            task.set_editor_property('destination_name',source.stem);task.set_editor_property('automated',True);task.set_editor_property('save',False);task.set_editor_property('replace_existing',asset is not None)
            if not texture:
                opt=unreal.FbxImportUI()
                for prop,val in [('import_mesh',True),('import_as_skeletal',False),('import_materials',False),('import_textures',False),('import_animations',False),('automated_import_should_detect_type',False)]:opt.set_editor_property(prop,val)
                opt.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
                data=opt.get_editor_property('static_mesh_import_data');data.set_editor_property('auto_generate_collision',False);data.set_editor_property('generate_lightmap_u_vs',False)
                data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
                # 默认保持旧作者行为；飞弹显式请求导入源顶点色，用于同槽分层而非新材质段。
                if globals().get('IMPORT_VERTEX_COLORS',False):data.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
                task.set_editor_property('options',opt)
            TOOLS.import_asset_tasks([task]);asset=unreal.load_asset(path)
        if texture:
            assert isinstance(asset,unreal.Texture2D)
            asset.set_editor_property('srgb',source.stem.endswith('_Color'))
            asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if source.stem.endswith('_Normal') else unreal.TextureCompressionSettings.TC_DEFAULT if source.stem.endswith('_Color') else unreal.TextureCompressionSettings.TC_MASKS)
            asset.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);asset.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
        else:
            assert isinstance(asset,unreal.StaticMesh)
            bounds=asset.get_bounds();report.append(dict(mesh=path,extentCm=[bounds.box_extent.x,bounds.box_extent.y,bounds.box_extent.z],materialSlots=len(asset.get_editor_property('static_materials'))))
        save(asset)
    (EVIDENCE/'ImportedSources.json').write_text(json.dumps(report,indent=2),encoding='utf8');print('SLASH_V2_IMPORTED',json.dumps(report))


def node(m,cls):return LIB.create_material_expression(m,cls)
def wire(a,out,b,pin):
    if not LIB.connect_material_expressions(a,out,b,pin):raise RuntimeError('材质连线失败：'+pin)
def output(n,prop,pin=''):
    if not LIB.connect_material_property(n,pin,prop):raise RuntimeError('材质输出失败：'+str(prop))
def constant(m,value):
    n=node(m,unreal.MaterialExpressionConstant);n.set_editor_property('r',value);return n
def scalar(m,name,value):
    n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def texture(m,name):
    n=node(m,unreal.MaterialExpressionTextureSampleParameter2D);n.set_editor_property('parameter_name',name)
    n.set_editor_property('texture',unreal.load_asset(BASE+'/Textures/T_DBA_SlashV2_'+name))
    n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if name=='Normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if name=='Color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS);return n


def materials():
    """保留色阶、低频流线、沿UV显露与消散；单个多材质Mesh，三个明确绘制层。"""
    for kind in ('Body','Streams','CloudRim'):
        name='M_DBA_AttackSlashV2_'+kind;path=BASE+'/Materials/'+name
        m=unreal.load_asset(path)
        if m is None:m=TOOLS.create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        else:LIB.delete_all_material_expressions(m)
        m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if kind=='Body' else unreal.BlendMode.BLEND_ADDITIVE)
        m.set_editor_property('two_sided',False)
        m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        LIB.set_base_material_usage(m,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True)
        uv=node(m,unreal.MaterialExpressionTextureCoordinate);pc=node(m,unreal.MaterialExpressionParticleColor);age=node(m,unreal.MaterialExpressionParticleRelativeTime)
        color=texture(m,'Color');masks=texture(m,'Masks')
        custom=node(m,unreal.MaterialExpressionCustom);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
        inputs=[]
        for n in ('UV','Clock','PC','Art','Masks'):
            i=unreal.CustomInput();i.set_editor_property('input_name',n);inputs.append(i)
        custom.set_editor_property('inputs',inputs)
        code='''float t=saturate(Clock);
float front=saturate(t/.26)*1.12;
float reveal=smoothstep(UV.x-.07,UV.x+.07,front);
float retreat=1-smoothstep(UV.x-.12,UV.x+.12,saturate((t-.68)/.32)*1.15);
float fade=saturate(t/.055)*saturate((1-t)/.18);
float3 rgb=Art.rgb;
float opacity=Masks.r;
'''
        if kind=='Body':code+='rgb*=1.15+Masks.g*.42+Masks.b*.50;opacity*=.92;\n'
        elif kind=='Streams':code+='rgb=lerp(float3(1,.25,.025),float3(1,.72,.26),UV.y);opacity=.80;\n'
        else:code+='rgb=lerp(float3(1,.35,.035),float3(1,.88,.40),smoothstep(.35,.95,UV.y));opacity=.92;\n'
        code+='return float4(rgb*PC.rgb,opacity*reveal*retreat*fade*PC.a);'
        custom.set_editor_property('code',code)
        for source,pin,dest in ((uv,'','UV'),(age,'','Clock'),(pc,'RGBA','PC'),(color,'RGB','Art'),(masks,'RGBA','Masks')):wire(source,pin,custom,dest)
        rgb=node(m,unreal.MaterialExpressionComponentMask);rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True);rgb.set_editor_property('a',False);wire(custom,'',rgb,'')
        alpha=node(m,unreal.MaterialExpressionComponentMask);alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True);wire(custom,'',alpha,'')
        strength=scalar(m,'EmissionStrength',2.4 if kind=='Body' else 4.0 if kind=='Streams' else 4.3)
        emiss=node(m,unreal.MaterialExpressionMultiply);wire(rgb,'',emiss,'A');wire(strength,'',emiss,'B')
        output(emiss,unreal.MaterialProperty.MP_EMISSIVE_COLOR);output(alpha,unreal.MaterialProperty.MP_OPACITY)
        LIB.layout_material_expressions(m);LIB.recompile_material(m);save(m)
    mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_AttackSlash_V2');slots=mesh.get_editor_property('static_materials')
    if len(slots)!=3:raise RuntimeError('FBX材质槽不符合三层合同：'+str(len(slots)))
    # FBX保留源材质名，按名绑定，不依赖导入排序。
    mapping={'Body':'Body','SilkStreams':'Streams','CloudAndRim':'CloudRim'}
    for index,slot in enumerate(slots):
        source=str(slot.material_slot_name);key=next((v for k,v in mapping.items() if k in source),None)
        if key is None:raise RuntimeError('未知源材质槽：'+source)
        mesh.set_material(index,unreal.load_asset(BASE+'/Materials/M_DBA_AttackSlashV2_'+key))
    save(mesh);print('SLASH_V2_MATERIALS_SAVED',len(slots))


def definition():
    name='DA_DBA_VFX_AttackSlashV2_Review';path=BASE+'/Definitions/'+name
    d=unreal.load_asset(path)
    if d is None:
        f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',unreal.GamePlatformVFXInstantDefinition)
        d=TOOLS.create_asset(name,BASE+'/Definitions',unreal.GamePlatformVFXInstantDefinition,f)
    ident=unreal.GamePlatformId();ident.set_editor_property('namespace','presentation.dba.common.attack.slash');ident.set_editor_property('name','review_v2');ident.set_editor_property('logical_version',1)
    d.set_editor_property('logical_id',ident);d.set_editor_property('niagara_system',unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V2'))
    d.set_editor_property('auto_destroy',True);d.set_editor_property('max_lifetime_seconds',3.);d.set_editor_property('allow_pooling',True);d.set_editor_property('require_fixed_bounds',True)
    params=unreal.GamePlatformVFXParameters();params.set_editor_property('float_parameters',{'User.Duration':.9,'User.Radius':230.,'User.Intensity':1.})
    d.set_editor_property('default_parameters',params);rules=[]
    for n,lo,hi in [('User.Duration',.25,2.),('User.Radius',100.,500.),('User.Intensity',.2,2.)]:
        rules.append(unreal.GamePlatformVFXParameterRule(name=n,type=unreal.GamePlatformVFXParameterType.FLOAT,min_value=lo,max_value=hi))
    schema=unreal.GamePlatformVFXParameterSchema(rules=rules);d.set_editor_property('parameter_schema',schema);save(d)
    print('SLASH_V2_NATIVE_DEFINITION_SAVED',path)


if PHASE=='import':import_sources()
elif PHASE=='materials':materials()
elif PHASE=='definition':definition()
elif PHASE=='library':pass
else:raise ValueError('未知阶段：'+PHASE)
