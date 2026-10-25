# -*- coding: utf-8 -*-
"""《神兽联盟》火球/冰块成形、飞行拖尾：建立只读源的四套独立审核基础资产（UE5.8）。
仅复制现有纹理、材质实例和真实Niagara系统；导入独立厚冰块OBJ。此为复用基础版，
不是已经完成四阶段发射器重做，也不是正式技能替换/PIE人工通过。无覆盖、无提交。
"""
import unreal,json,hashlib,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
B='/DBAPresentationPack_Core/VFX'
DEST=B+'/OrbProjectile/Review/V1'
SRC=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/OrbProjectile/Review/V1'
RESULT=ROOT/'Saved/Validation/OrbProjectileV1/ReviewAssetFoundation.json'
textures={
 'T_DBA_FireOrb_FlameFlip_Review':B+'/Library/Game/FireMage/Textures/T_PY_FlameFlipbook',
 'T_DBA_FireOrb_RibbonFlame_Review':B+'/Library/00_Core/Textures/Fire/T_Fire_FlameRibbon',
 'T_DBA_FireOrb_TrailGradient_Review':B+'/Library/00_Core/Textures/ProjectileTrail/T_VFX_Trail_Gradient_01',
 'T_DBA_IceBlock_CrackMask_Review':B+'/IceArrow/Textures/T_DBA_IceArrow_CrackMask',
 'T_DBA_IceBlock_Normal_Review':B+'/IceArrow/Textures/T_DBA_IceArrow_IceNormal',
 'T_DBA_IceBlock_FrostMist_Review':B+'/IceArrow/Textures/T_DBA_IceArrow_FrostMist',
 'T_DBA_IceBlock_TrailStrip_Review':B+'/IceArrow/Textures/T_DBA_IceArrow_TrailStrip'
}
material_instances={
 'MI_DBA_FireOrb_Form_Core_Review':B+'/Library/00_Core/Materials/Fire/MI_Fire_FlipbookCore',
 'MI_DBA_FireOrb_Form_Tongues_Review':B+'/Library/00_Core/Materials/Fire/MI_Fire_FlipbookCore',
 'MI_DBA_FireOrb_Form_Ember_Review':B+'/Library/00_Core/Materials/Fire/MI_Fire_Ember',
 'MI_DBA_FireOrb_Flight_Core_Review':B+'/Library/00_Core/Materials/Fire/MI_Fire_FlipbookCore',
 'MI_DBA_FireOrb_Flight_Ribbon_Review':B+'/Library/00_Core/Materials/ProjectileTrail/MI_VFX_ProjectileTrail_SoftCrimson',
 'MI_DBA_FireOrb_Flight_Ember_Review':B+'/Library/00_Core/Materials/Fire/MI_Fire_Ember',
 'MI_DBA_IceBlock_Form_Crystal_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_Crystal',
 'MI_DBA_IceBlock_Form_Mist_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_Mist',
 'MI_DBA_IceBlock_Form_Frost_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_CrystalSprite',
 'MI_DBA_IceBlock_Flight_Crystal_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_Crystal',
 'MI_DBA_IceBlock_Flight_Ribbon_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_Trail',
 'MI_DBA_IceBlock_Flight_Mist_Review':B+'/IceArrow/Materials/MI_DBA_IceArrow_Mist'
}
systems={
 'NS_DBA_FireOrb_Form_BaselineReview':B+'/Library/01_Cast/NS_Fire_Fireball_Charge',
 'NS_DBA_IceBlock_Form_BaselineReview':B+'/IceArrow/Niagara/NS_DBA_IceArrow_Flight',
 'NS_DBA_FireOrb_Flight_BaselineReview':B+'/DevelopmentMagic/Niagara/NS_DBA_DevFireballFlight',
 'NS_DBA_IceBlock_Flight_BaselineReview':B+'/IceArrow/Niagara/NS_DBA_IceArrow_Flight',
}
REVIEW={
 'engine':unreal.SystemLibrary.get_engine_version(),'project':unreal.Paths.get_project_file_path(),
 'status':'preflight','created':[],'sources':{'textures':textures,'materials':material_instances,'systems':systems},
 'validation':{},'errors':[],'visualApproved':False,'authoritativeProjectileUntouched':True,
 'serverTimingUntouched':True,'statusExplanation':'These are copied starting points, not completed final Niagara structures.'
}
try:
    assert REVIEW['engine'].startswith('5.8')
    assert REVIEW['project'].replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    taskpath=SRC/'SM_DBA_IceBlock_FacetedCore_Review.obj'
    manifest=SRC/'IceBlockFacetedCore_ReviewManifest.json'
    assert taskpath.is_file() and manifest.is_file()
    desc=json.loads(manifest.read_text(encoding='utf8'))
    assert hashlib.sha256(taskpath.read_bytes()).hexdigest()==desc['sha256']
    assert desc['triangles']>=20,'block mesh too simple'
    targets=[*(DEST+'/Textures/'+k for k in textures),*(DEST+'/Materials/'+k for k in material_instances),
             *(DEST+'/Niagara/Systems/'+k for k in systems),DEST+'/Meshes/SM_DBA_IceBlock_FacetedCore_Review']
    for path in targets:
        if unreal.EditorAssetLibrary.does_asset_exist(path):raise RuntimeError('拒绝覆盖已有审核资源: '+path)
    # Verify original references before any writes
    for category,items,expected in [('textures',textures,unreal.Texture2D),
                                    ('materials',material_instances,unreal.MaterialInstanceConstant),
                                    ('systems',systems,unreal.NiagaraSystem)]:
        for name,src in items.items():
            obj=unreal.load_asset(src)
            if not isinstance(obj,expected):raise RuntimeError('复用源类型或路径错误 '+category+' '+src)
    REVIEW['status']='importing_mesh'
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(taskpath))
    task.set_editor_property('destination_path',DEST+'/Meshes')
    task.set_editor_property('destination_name','SM_DBA_IceBlock_FacetedCore_Review')
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',False)
    task.set_editor_property('save',False)
    try:
        options=unreal.FbxImportUI()
        options.set_editor_property('import_materials',False)
        options.set_editor_property('import_textures',False)
        options.set_editor_property('import_as_skeletal',False)
        task.set_editor_property('options',options)
    except Exception as e:REVIEW['validation']['importOptionsWarning']=str(e)[:500]
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh=unreal.load_asset(DEST+'/Meshes/SM_DBA_IceBlock_FacetedCore_Review')
    if not isinstance(mesh,unreal.StaticMesh):raise RuntimeError('真实冰块Mesh导入失败')
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh,False):raise RuntimeError('Mesh保存失败')
    REVIEW['created'].append(mesh.get_path_name())
    try:
        bounds=mesh.get_bounds()
        REVIEW['validation']['iceMeshBounds']=str(bounds)[:500]
    except Exception:pass
    REVIEW['status']='duplicating_textures'
    def duplicate(src,dest,typ):
        obj=unreal.EditorAssetLibrary.duplicate_asset(src,dest)
        if not isinstance(obj,typ):raise RuntimeError('原生资产复制失败 '+dest)
        if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('保存复制资产失败 '+dest)
        REVIEW['created'].append(dest)
        return obj
    for name,src in textures.items():
        duplicate(src,DEST+'/Textures/'+name,unreal.Texture2D)
    REVIEW['status']='duplicating_materials'
    for name,src in material_instances.items():
        obj=duplicate(src,DEST+'/Materials/'+name,unreal.MaterialInstanceConstant)
        scalar=[str(v) for v in unreal.MaterialEditingLibrary.get_scalar_parameter_names(obj)]
        REVIEW['validation'][name+'ScalarParams']=scalar
        # Only set parameters that already exist on copied MI parent; never change platform parent materials.
        vals={}
        if name.startswith('MI_DBA_FireOrb_'):
            vals={'Energy':2.9 if 'Core' in name else 2.15,'Density':.82 if 'Core' in name else .67}
        elif '_Mist_' in name:
            vals={'EmissiveStrength':1.15,'OpacityScale':.44}
        elif '_Crystal_' in name:
            vals={'EmissiveStrength':.75,'OpacityScale':.78}
        for k,v in vals.items():
            if k in scalar:
                unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(obj,k,float(v))
        if not unreal.EditorAssetLibrary.save_loaded_asset(obj,False):raise RuntimeError('材料参数保存失败 '+name)
    REVIEW['status']='duplicating_niagara'
    for name,src in systems.items():
        obj=duplicate(src,DEST+'/Niagara/Systems/'+name,unreal.NiagaraSystem)
        REVIEW['validation'][name]={'exists':True,'compileVerified':False,'rendererReconfigured':False,'humanAccepted':False}
    REVIEW['status']='review_foundations_created'
    print('ORB_PROJECTILE_V1_FOUNDATIONS',len(REVIEW['created']),str(RESULT),flush=True)
except Exception:
    REVIEW['status']='failed'
    REVIEW['errors'].append(traceback.format_exc()[-4500:])
    print('ORB_PROJECTILE_V1_FOUNDATION_FAILED',REVIEW['errors'][-1],flush=True)
    raise
finally:
    RESULT.parent.mkdir(parents=True,exist_ok=True)
    RESULT.write_text(json.dumps(REVIEW,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
