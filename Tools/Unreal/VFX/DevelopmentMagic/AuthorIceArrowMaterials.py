#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM冰箭UE5.8原生导入和材质作者；由Monolith run_python在所属编辑器执行。
先import导入源PNG/OBJ，再materials创建参数化母材质及实例。仅写IceArrow独占内容目录；
已有同名材质拒绝重建，避免覆盖审核成果。源码图不是.uasset；所有资产由UE保存。
不连接后端、不处理技能伤害；输出门禁仍须真实着色器编译和独立重载验证。
"""
from pathlib import Path
import argparse, json
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/IceArrow'
DEST='/DBAPresentationPack_Core/VFX/IceArrow'
LIB=unreal.MaterialEditingLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()

def check():
    """只允许锁定工程中明确属于本任务的编辑器实例；拒绝后台生产或其它工程。"""
    project=unreal.Paths.get_project_file_path().replace('\\','/')
    if not project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject') or '-IceArrowOwnedAuthoring' not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError('不是本任务拥有的SSLM编辑器')
    if not unreal.SystemLibrary.get_engine_version().startswith('5.8.'):raise RuntimeError('未使用锁定UE5.8')

def save(asset):
    """显式保存并回读；失败立即停止，不把内存对象记为磁盘成果。"""
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False):raise RuntimeError('原生资产保存失败：'+asset.get_path_name())
    loaded=unreal.load_asset(asset.get_path_name())
    if loaded is None:raise RuntimeError('原生资产回读失败')
    print('ICE_ARROW_NATIVE_SAVED',asset.get_path_name())

def import_sources():
    """真实导入技术纹理及原创OBJ：遮罩/法线采用线性采样，所有网格禁止生成碰撞。"""
    sources=sorted(SOURCE.glob('*.png'))+sorted(SOURCE.glob('*.obj'))
    if len(sources)!=15:raise RuntimeError('源资产数量不符合已审核的11纹理+4网格')
    for source in sources:
        is_texture=source.suffix=='.png';folder=DEST+('/Textures' if is_texture else '/Meshes')
        path=folder+'/'+source.stem
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            raise RuntimeError('同名原生资产已存在，先审核而非覆盖：'+path)
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
        task.set_editor_property('destination_path',folder);task.set_editor_property('destination_name',source.stem)
        task.set_editor_property('automated',True);task.set_editor_property('save',False);task.set_editor_property('replace_existing',False)
        if not is_texture:
            options=unreal.FbxImportUI();options.set_editor_property('import_mesh',True)
            options.set_editor_property('import_as_skeletal',False);options.set_editor_property('import_animations',False)
            options.set_editor_property('import_materials',False);options.set_editor_property('import_textures',False)
            options.set_editor_property('automated_import_should_detect_type',False)
            options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            data=options.get_editor_property('static_mesh_import_data')
            data.set_editor_property('generate_lightmap_u_vs',False);data.set_editor_property('auto_generate_collision',False)
            data.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            task.set_editor_property('options',options)
        TOOLS.import_asset_tasks([task]);asset=unreal.load_asset(path)
        if asset is None:raise RuntimeError('原生导入未产生目标资产：'+path)
        if is_texture:
            if not isinstance(asset,unreal.Texture2D):raise RuntimeError('导入类型不是Texture2D')
            asset.set_editor_property('srgb',False)
            asset.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if source.stem.endswith('IceNormal') else unreal.TextureCompressionSettings.TC_MASKS)
            if source.stem.endswith(('IceNormal','IceORM','CrackMask')):
                asset.set_editor_property('address_x',unreal.TextureAddress.TA_WRAP);asset.set_editor_property('address_y',unreal.TextureAddress.TA_WRAP)
            else:
                asset.set_editor_property('address_x',unreal.TextureAddress.TA_CLAMP);asset.set_editor_property('address_y',unreal.TextureAddress.TA_CLAMP)
        elif not isinstance(asset,unreal.StaticMesh):raise RuntimeError('导入类型不是StaticMesh')
        save(asset)
    print('ICE_ARROW_NATIVE_IMPORT_COMPLETED',len(sources))

def node(mat,cls,x=0,y=0):return LIB.create_material_expression(mat,cls,x,y)
def connect(a,out,b,inp):
    """UE表达式连接必须真实成功；尤其空名称引脚不允许默默忽略。"""
    if not LIB.connect_material_expressions(a,out,b,inp):raise RuntimeError('表达式连接失败：'+b.get_name()+'/'+inp)
def output(n,p,pin=''):
    if not LIB.connect_material_property(n,pin,p):raise RuntimeError('材质输出连接失败：'+str(p))
def scalar(mat,name,value,x=0,y=0):
    n=node(mat,unreal.MaterialExpressionScalarParameter,x,y);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(mat,name,color,x=0,y=0):
    n=node(mat,unreal.MaterialExpressionVectorParameter,x,y);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*color));return n
def mul(mat,a,b,ao='',bo='',x=0,y=0):
    n=node(mat,unreal.MaterialExpressionMultiply,x,y);connect(a,ao,n,'A');connect(b,bo,n,'B');return n
def texture(mat,name,asset_name,x=-700,y=0,normal=False):
    asset=unreal.load_asset(DEST+'/Textures/T_DBA_IceArrow_'+asset_name)
    if not isinstance(asset,unreal.Texture2D):raise RuntimeError('缺少已导入贴图：'+asset_name)
    n=node(mat,unreal.MaterialExpressionTextureSampleParameter2D,x,y);n.set_editor_property('parameter_name',name);n.set_editor_property('texture',asset)
    n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    return n

def new_material(name,blend,lit=False):
    """只创建新母材质；颜色、强度及消隐均参数化，供项目实例覆盖。"""
    if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/Materials/'+name):raise RuntimeError('拒绝覆盖已有母材质：'+name)
    m=TOOLS.create_asset(name,DEST+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(m,unreal.Material):raise RuntimeError('母材质创建失败')
    m.set_editor_property('two_sided',True);m.set_editor_property('blend_mode',blend)
    m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT if lit else unreal.MaterialShadingModel.MSM_UNLIT)
    for usage in (unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES,unreal.MaterialUsage.MATUSAGE_NIAGARA_RIBBONS):
        LIB.set_base_material_usage(m,usage,True)
    return m

def make_sprite(name,mask,additive=True,scroll=False):
    """透明卡片母材质：纹理Alpha×粒子Alpha×接触柔化，杜绝黑白背景方片。"""
    m=new_material(name,unreal.BlendMode.BLEND_ADDITIVE if additive else unreal.BlendMode.BLEND_TRANSLUCENT)
    tex=texture(m,'MaskTexture',mask);pc=node(m,unreal.MaterialExpressionParticleColor,-500,-220)
    tint=vector(m,'IceTint',(.11,.62,1.0,1),-450,-400);strength=scalar(m,'EmissiveStrength',2.2,-200,-330)
    color=mul(m,pc,tint,'RGB','RGB',-170,-200);emissive=mul(m,color,strength,x=15,y=-150)
    output(emissive,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    density=scalar(m,'OpacityScale',.85,-150,320);alpha=mul(m,tex,pc,'A','A',-350,150);alpha=mul(m,alpha,density,x=-100,y=170)
    fade=node(m,unreal.MaterialExpressionDepthFade,80,170);fade.set_editor_property('fade_distance_default',12.0);connect(alpha,'',fade,'Opacity')
    output(fade,unreal.MaterialProperty.MP_OPACITY)
    if scroll:
        uv=node(m,unreal.MaterialExpressionTextureCoordinate,-1050,120);panner=node(m,unreal.MaterialExpressionPanner,-870,120)
        panner.set_editor_property('speed_x',-.75);connect(uv,'',panner,'Coordinate');connect(panner,'',tex,'UVs')
    LIB.recompile_material(m);save(m);return m

def make_crystal():
    """冰晶表面：真实法线+粗糙度贴图+可调裂纹发光，生命周期由ParticleColor控制。"""
    m=new_material('M_DBA_IceArrow_Crystal',unreal.BlendMode.BLEND_TRANSLUCENT,True)
    m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE)
    pc=node(m,unreal.MaterialExpressionParticleColor,-650,-250);tint=vector(m,'IceTint',(.045,.28,.48,1),-650,-450)
    base=mul(m,pc,tint,'RGB','RGB',-350,-250);output(base,unreal.MaterialProperty.MP_BASE_COLOR)
    normal=texture(m,'IceNormal','IceNormal',-900,210,True);output(normal,unreal.MaterialProperty.MP_NORMAL,'RGB')
    orm=texture(m,'IceORM','IceORM',-900,430);output(orm,unreal.MaterialProperty.MP_ROUGHNESS,'G');output(orm,unreal.MaterialProperty.MP_METALLIC,'B')
    cracks=texture(m,'CrackMask','CrackMask',-900,650)
    fresnel=node(m,unreal.MaterialExpressionFresnel,-630,770);fresnel.set_editor_property('exponent',3.2);fresnel.set_editor_property('base_reflect_fraction',.03)
    edge=node(m,unreal.MaterialExpressionAdd,-350,580);connect(cracks,'R',edge,'A');connect(fresnel,'',edge,'B')
    glow=vector(m,'EdgeTint',(.15,.80,1.9,1),-320,840);intensity=scalar(m,'EdgeIntensity',1.35,-100,950)
    emission=mul(m,edge,glow,'','RGB',-40,550);emission=mul(m,emission,intensity,x=150,y=530)
    output(emission,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity=scalar(m,'CrystalOpacity',.80,-320,50);alpha=mul(m,pc,opacity,'A','',-90,70);output(alpha,unreal.MaterialProperty.MP_OPACITY)
    spec=scalar(m,'SpecularLevel',.65,-50,270);output(spec,unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(m);save(m);return m

def instance(name,parent,mask=None,strength=2.2,density=.85):
    """实例只覆盖必要参数，母材质不复制；存在同名实例时停止并要求明确审核。"""
    path=DEST+'/Materials/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('实例已存在：'+path)
    m=TOOLS.create_asset(name,DEST+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(m,parent)
    if mask: LIB.set_material_instance_texture_parameter_value(m,'MaskTexture',unreal.load_asset(DEST+'/Textures/T_DBA_IceArrow_'+mask))
    LIB.set_material_instance_scalar_parameter_value(m,'EmissiveStrength',strength);LIB.set_material_instance_scalar_parameter_value(m,'OpacityScale',density)
    LIB.update_material_instance(m);save(m)

def materials():
    """建立四母材质、十实例，不用AI材质球预览图冒充真实材质。"""
    additive=make_sprite('M_DBA_IceArrow_Luminous','ImpactFlash',True)
    mist=make_sprite('M_DBA_IceArrow_Mist','FrostMist',False)
    trail=make_sprite('M_DBA_IceArrow_Trail','TrailStrip',True,True)
    crystal=make_crystal()
    specs=[('GroundOuter',additive,'RuneOuter',3.4,.95),('GroundInner',additive,'RuneInner',4.0,.95),
      ('CrystalSprite',additive,'CrystalShard',1.7,.70),('Snow',additive,'IceFlake',1.3,.7),
      ('ImpactFlash',additive,'ImpactFlash',5.0,.95),('FrozenGround',additive,'FrozenGround',2.1,.8),
      ('Mist',mist,'FrostMist',.75,.25),('Trail',trail,'TrailStrip',2.4,.65),
      ('Crystal',crystal,None,1.,1.),('ImpactCrystal',crystal,None,1.,1.)]
    for n,parent,mask,power,density in specs:instance('MI_DBA_IceArrow_'+n,parent,mask,power,density)
    print('ICE_ARROW_NATIVE_MATERIALS_COMPLETED','masters=4','instances=10')

if __name__=='__main__':
    check();p=argparse.ArgumentParser();p.add_argument('--phase',required=True,choices=('import','materials'));args=p.parse_args()
    import_sources() if args.phase=='import' else materials()
