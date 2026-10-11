# -*- coding: utf-8 -*-
"""UE5.8平台PBR母材质采样器修复：TC_MASKS匹配SAMPLERTYPE_MASKS。
仅在独立游戏验收证明修复有效且无其他编辑器占用后调用；
只修改平台母材质并创建平台中立遮罩Texture2D，项目15份MI引用保持不变。
"""
from pathlib import Path
import hashlib,json,shutil,unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
MASTER='/GamePlatformVFX/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage'
MASK='/GamePlatformVFX/Environment/AmbientNature/Textures/T_GP_Ambient_DefaultMaskWhite'
SOURCE=ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/AmbientNature/Textures/T_GP_Ambient_DefaultMaskWhite_Source.png'
DISK=ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage.uasset'
BACKUP=ROOT/'Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBackup/M_GP_AmbientNature_PBR_Foliage_PreMaskSampler.uasset'
REPORT=ROOT/'Saved/Validation/FallingFoliage/PBRAtlas20261011/PBRMasterMaskSamplerRepair.json'
EXPECTED_SOURCE='ef36b6025c3548a5d035b22d93a15d7ff5ea4e04352ef5f89bd717c7c7862e91'
MI_ROOT='/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Materials/MI_DBA_Ambient_'
STYLES=('OsmanthusBlossom','RosePetal','GenericPetalPink','GenericPetalWhite','GenericPetalRed',
        'RedSpiderLilyPetal','PearPetal','CherryPetal','PlumPetal','PlaneLeaf',
        'WillowLeaf','PoplarLeaf','OakLeaf','BirchLeaf','YellowLeaf')
assets=unreal.EditorAssetLibrary
lib=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
report={'source':str(SOURCE),'master':MASTER,'mask':MASK,'patched':[],
        'miReadback':[],'status':'not_started','errors':[]}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
try:
    project=str(unreal.Paths.get_project_file_path()).replace('\\','/')
    if not project.endswith('/DivineBeastsArena.uproject') or not str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'):
        raise RuntimeError('仅可在神兽联盟UE5.8官方工程运行')
    if not SOURCE.is_file() or sha(SOURCE)!=EXPECTED_SOURCE or not DISK.is_file():
        raise RuntimeError('平台源纹理SHA不符，或者真实母材质.uasset不存在')
    m=assets.load_asset(MASTER)
    if not isinstance(m,unreal.Material):raise RuntimeError('平台PBR母材质不是Material')
    if assets.does_asset_exist(MASK):
        white=assets.load_asset(MASK)
        if not isinstance(white,unreal.Texture2D) or white.get_editor_property('srgb') or white.get_editor_property('compression_settings')!=unreal.TextureCompressionSettings.TC_MASKS:
            raise RuntimeError('已有中立Mask资源类型/压缩不符合平台合同')
    else:
        task=unreal.AssetImportTask()
        for k,v in (('filename',str(SOURCE)),('destination_path',MASK.rsplit('/',1)[0]),
                    ('destination_name',MASK.rsplit('/',1)[1]),('automated',True),
                    ('replace_existing',False),('save',False)):
            task.set_editor_property(k,v)
        tools.import_asset_tasks([task])
        white=assets.load_asset(MASK)
        if not isinstance(white,unreal.Texture2D):raise RuntimeError('平台中立Mask导入失败')
        white.set_editor_property('srgb',False)
        white.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
        if not assets.save_loaded_asset(white,only_if_is_dirty=False):raise RuntimeError('平台中立Mask保存失败')
        report['whiteTextureCreated']=True
    matched=[]
    for e in lib.get_material_expressions(m):
        if not isinstance(e,unreal.MaterialExpressionTextureSampleParameter2D):continue
        n=str(e.get_editor_property('parameter_name'))
        if n not in ('LeafOpacity','LeafRoughnessMap'):continue
        matched.append(n)
        sampler=e.get_editor_property('sampler_type')
        old=e.get_editor_property('texture')
        if sampler==unreal.MaterialSamplerType.SAMPLERTYPE_MASKS and old==white:continue
        if sampler!=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR or not old or not old.get_path_name().startswith('/Engine/EngineResources/DefaultTexture.'):
            raise RuntimeError('原采样节点已被其他编辑器修改，禁止覆盖:'+n)
        if not BACKUP.exists():
            BACKUP.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(DISK,BACKUP)
        if sha(BACKUP)!=sha(DISK):
            raise RuntimeError('平台母材质现有文件与备份SHA-256不一致，拒绝修改')
        e.set_editor_property('texture',white)
        e.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        if e.get_editor_property('sampler_type')!=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS:
            raise RuntimeError('PBR采样器未正确回读:'+n)
        report['patched'].append(n)
    if set(matched)!={'LeafOpacity','LeafRoughnessMap'}:raise RuntimeError('平台PBR母材质参数结构异常')
    if report['patched']:
        errors=lib.recompile_material(m)
        if errors and any('error' in str(x).lower() for x in errors):
            raise RuntimeError('PBR母材质编译失败:'+str(errors))
        if not assets.save_loaded_asset(m,only_if_is_dirty=False):raise RuntimeError('PBR母材质保存失败')
    for species in STYLES:
        mi=assets.load_asset(MI_ROOT+species)
        if not isinstance(mi,unreal.MaterialInstanceConstant) or mi.get_editor_property('parent')!=m:
            raise RuntimeError('原项目实例没有继承平台母材质:'+species)
        for attr in ('LeafOpacity','LeafRoughnessMap'):
            texture=lib.get_material_instance_texture_parameter_value(mi,unreal.Name(attr))
            if not isinstance(texture,unreal.Texture2D) or texture.get_editor_property('compression_settings')!=unreal.TextureCompressionSettings.TC_MASKS:
                raise RuntimeError('项目实例Mask纹理不符合平台合同:'+species+'/'+attr)
        report['miReadback'].append(species)
    report['status']='saved' if report['patched'] else 'already_correct'
    print('PBR_MASTER_MASK_SAMPLERS',report['status'],'patched',report['patched'],
          'project_instances_checked',len(report['miReadback']),flush=True)
except Exception as ex:
    report['status']='failed'
    report['errors'].append(str(ex))
    print('PBR_MASTER_MASK_SAMPLERS_ERROR',str(ex),flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
