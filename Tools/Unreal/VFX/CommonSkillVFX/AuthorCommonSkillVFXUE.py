# -*- coding: utf-8 -*-
"""Monolith引擎内作者：真实导入通用VFX网格、创建共享材质与平台Definition。

只修改本轮新建CommonSkillVFX目录，不接触地图、既有治疗或生肖技能。
由外部Monolith配方注入PHASE/KEYS，失败终止；保存的是UE原生资产。
"""
from pathlib import Path
import json
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core'
SOURCE=PACK/'SourceArt/VFX/CommonSkillVFX/V1'
DEST='/DBAPresentationPack_Core/VFX/CommonSkillVFX'
REPORT=ROOT/'Saved/Validation/CommonSkillVFX/Authoring'
REPORT.mkdir(parents=True,exist_ok=True)
assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
ROWS=json.loads((SOURCE/'Sources.json').read_text(encoding='utf8'))['templates']
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary


def save(asset):
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('保存失败：'+asset.get_path_name())


def import_meshes():
    """OBJ带厘米坐标与顶点法线，无碰撞、无伪贴图；同名已存在只回读本轮独占路径。"""
    done=[]
    for row in ROWS:
        if globals().get('KEYS') and row['key'] not in KEYS:continue
        for part in row['meshes']:
            source=SOURCE/part['file'];path=DEST+'/Meshes/'+source.stem
            mesh=unreal.load_asset(path)
            if mesh is None:
                task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
                task.set_editor_property('destination_path',DEST+'/Meshes');task.set_editor_property('destination_name',source.stem)
                task.set_editor_property('automated',True);task.set_editor_property('save',False);task.set_editor_property('replace_existing',False)
                opt=unreal.FbxImportUI()
                for prop,value in [('import_mesh',True),('import_as_skeletal',False),('import_animations',False),('import_materials',False),('import_textures',False),('automated_import_should_detect_type',False)]:opt.set_editor_property(prop,value)
                opt.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
                data=opt.get_editor_property('static_mesh_import_data')
                data.set_editor_property('generate_lightmap_u_vs',False);data.set_editor_property('auto_generate_collision',False)
                data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
                task.set_editor_property('options',opt);TOOLS.import_asset_tasks([task]);mesh=unreal.load_asset(path)
            if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('导入类型不符：'+path)
            save(mesh);done.append(path)
    (REPORT/'ImportedMeshes.json').write_text(json.dumps(done,indent=2),encoding='utf8')
    print('COMMON_NATIVE_MESHES',len(done))


def node(mat,cls):return LIB.create_material_expression(mat,cls)
def scalar(mat,name,value):
    n=node(mat,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def wire(a,out,b,slot):
    if not LIB.connect_material_expressions(a,out,b,slot):raise RuntimeError('材质连接失败：'+slot)
def product(mat,a,b,ao='',bo=''):
    n=node(mat,unreal.MaterialExpressionMultiply);wire(a,ao,n,'A');wire(b,bo,n,'B');return n
def output(n,prop,pin=''):
    if not LIB.connect_material_property(n,pin,prop):raise RuntimeError('材质输出失败')


def materials():
    """三个共享无噪声母材质；能量、透明护罩与金属边缘分别有真实PBR职责。"""
    for kind in ('Energy','Shell','Rim'):
        path=DEST+'/Materials/M_DBA_CommonVFX_'+kind
        if unreal.EditorAssetLibrary.does_asset_exist(path):continue
        mat=TOOLS.create_asset(path.rsplit('/',1)[1],DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
        mat.set_editor_property('two_sided',True);mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        mat.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE)
        LIB.set_base_material_usage(mat,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,True)
        pc=node(mat,unreal.MaterialExpressionParticleColor)
        output(pc,unreal.MaterialProperty.MP_BASE_COLOR,'RGB')
        emission=product(mat,pc,scalar(mat,'Emission',.7 if kind=='Shell' else 1.25 if kind=='Energy' else .55),'RGB')
        output(emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        alpha=product(mat,pc,scalar(mat,'Opacity',.55 if kind=='Energy' else .84 if kind=='Rim' else .44),'A')
        if kind=='Shell':
            fres=node(mat,unreal.MaterialExpressionFresnel);fres.set_editor_property('exponent',2.5);fres.set_editor_property('base_reflect_fraction',.07)
            alpha=product(mat,alpha,fres)
        output(alpha,unreal.MaterialProperty.MP_OPACITY)
        output(scalar(mat,'Roughness',.24 if kind=='Rim' else .32),unreal.MaterialProperty.MP_ROUGHNESS)
        output(scalar(mat,'Specular',.65),unreal.MaterialProperty.MP_SPECULAR)
        output(scalar(mat,'Metallic',.45 if kind=='Rim' else .04),unreal.MaterialProperty.MP_METALLIC)
        LIB.layout_material_expressions(mat);LIB.recompile_material(mat);save(mat)
    print('COMMON_NATIVE_MASTER_MATERIALS',3)


def linear(hexvalue):
    values=[int(hexvalue[i:i+2],16)/255 for i in (0,2,4)]
    return unreal.LinearColor(*[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in values],1)


def definitions():
    """20份平台原生可加载Definition，真实软引用、默认参数、白名单与有界寿命。"""
    report=[]
    effect_paths=[a for a in unreal.EditorAssetLibrary.list_assets('/GamePlatformVFX',True,False) if '/NET_GP_' in a]
    for row in ROWS:
        key=row['key'];cls=getattr(unreal,'GamePlatformVFX'+row['behavior']+'Definition')
        path=DEST+'/Definitions/DA_DBA_VFX_Common_'+key;asset=unreal.load_asset(path)
        if asset is None:
            factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',cls)
            asset=TOOLS.create_asset(path.rsplit('/',1)[1],DEST+'/Definitions',cls,factory)
        if not isinstance(asset,cls):raise RuntimeError('Definition类型错误')
        namespace,tail=row['definitionId'].rsplit('.',1);name,version=tail.split('@')
        identity=unreal.GamePlatformId();identity.set_editor_property('namespace',namespace);identity.set_editor_property('name',name);identity.set_editor_property('logical_version',int(version))
        asset.set_editor_property('logical_id',identity)
        system=unreal.load_asset(DEST+'/Niagara/NS_DBA_Common_'+key)
        if not isinstance(system,unreal.NiagaraSystem):raise RuntimeError('System不存在：'+key)
        asset.set_editor_property('niagara_system',system);asset.set_editor_property('auto_destroy',True);asset.set_editor_property('allow_pooling',True)
        asset.set_editor_property('require_fixed_bounds',True);asset.set_editor_property('max_lifetime_seconds',12.)
        asset.set_editor_property('use_shared_burst_channel',False)
        params=unreal.GamePlatformVFXParameters()
        floats={'User.Duration':row['duration'],'User.Radius':120.,'User.Length':400.,'User.Width':120.,'User.Speed':400.,'User.Intensity':1.}
        params.set_editor_property('float_parameters',floats)
        params.set_editor_property('color_parameters',{'User.PrimaryColor':linear(row['referenceSRGB']),'User.SecondaryColor':linear('D5AA65' if row['category'] in ('Attack','Defense') else row['referenceSRGB'])})
        asset.set_editor_property('default_parameters',params)
        schema=unreal.GamePlatformVFXParameterSchema();rules=[]
        for n,(minimum,maximum) in {'User.Duration':(.15,10.),'User.Radius':(20.,400.),'User.Length':(50.,800.),'User.Width':(20.,300.),'User.Speed':(0.,2000.),'User.Intensity':(.1,3.)}.items():
            rule=unreal.GamePlatformVFXParameterRule();rule.set_editor_property('name',n);rule.set_editor_property('type',unreal.GamePlatformVFXParameterType.FLOAT)
            rule.set_editor_property('min_value',minimum);rule.set_editor_property('max_value',maximum);rules.append(rule)
        for n in ('User.PrimaryColor','User.SecondaryColor'):
            rule=unreal.GamePlatformVFXParameterRule();rule.set_editor_property('name',n);rule.set_editor_property('type',unreal.GamePlatformVFXParameterType.COLOR);rules.append(rule)
        schema.set_editor_property('rules',rules);asset.set_editor_property('parameter_schema',schema)
        # 原生EffectType存在时明确复用；不存在由审计记录，禁止假称已绑定。
        wanted='NET_GP_CombatStatus' if row['behavior'] in ('Attached','Shield') else 'NET_GP_CombatArea' if row['behavior']=='Area' else 'NET_GP_CombatProjectile' if row['behavior'] in ('Projectile','Beam','Trail') else 'NET_GP_CombatImpact'
        match=[p for p in effect_paths if p.rsplit('/',1)[-1].split('.')[0]==wanted]
        if match:asset.set_editor_property('effect_type',unreal.load_asset(match[0]));system.set_editor_property('effect_type',unreal.load_asset(match[0]));save(system)
        save(asset);report.append(dict(key=key,definition=path,logicalId=row['definitionId'],system=system.get_path_name(),effectType=match[:1],manualAccepted=False))
    (REPORT/'Definitions.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('COMMON_NATIVE_DEFINITIONS',len(report))


if PHASE=='import':import_meshes()
elif PHASE=='materials':materials()
elif PHASE=='definitions':definitions()
else:raise ValueError('未知阶段：'+PHASE)
