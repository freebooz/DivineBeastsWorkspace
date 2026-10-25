# -*- coding: utf-8 -*-
"""SSLM OrbProjectile四阶段基础审核资源，UE5.8独立进程真实冷重载、Niagara脚本编译核查。
不会修改任何asset；baseline仅是复用起点，非正式完成的4套粒子表现。"""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
OUT=ROOT/'Saved/Validation/OrbProjectileV1/FoundationColdLoad.json'
system_names=['NS_DBA_FireOrb_Form_BaselineReview','NS_DBA_IceBlock_Form_BaselineReview',
              'NS_DBA_FireOrb_Flight_BaselineReview','NS_DBA_IceBlock_Flight_BaselineReview']
material_names=['MI_DBA_FireOrb_Form_Core_Review','MI_DBA_FireOrb_Form_Tongues_Review',
'MI_DBA_FireOrb_Form_Ember_Review','MI_DBA_FireOrb_Flight_Core_Review',
'MI_DBA_FireOrb_Flight_Ribbon_Review','MI_DBA_FireOrb_Flight_Ember_Review',
'MI_DBA_IceBlock_Form_Crystal_Review','MI_DBA_IceBlock_Form_Mist_Review',
'MI_DBA_IceBlock_Form_Frost_Review','MI_DBA_IceBlock_Flight_Crystal_Review',
'MI_DBA_IceBlock_Flight_Ribbon_Review','MI_DBA_IceBlock_Flight_Mist_Review']
texture_names=['T_DBA_FireOrb_FlameFlip_Review','T_DBA_FireOrb_RibbonFlame_Review',
'T_DBA_FireOrb_TrailGradient_Review','T_DBA_IceBlock_CrackMask_Review',
'T_DBA_IceBlock_Normal_Review','T_DBA_IceBlock_FrostMist_Review',
'T_DBA_IceBlock_TrailStrip_Review']
report={'engine':unreal.SystemLibrary.get_engine_version(),'project':unreal.Paths.get_project_file_path(),
 'status':'started','systems':{},'textures':{},'materials':{},'mesh':{},'errors':[],
 'newVisualStructureImplemented':False,'runtimeAttached':False,'manualVisualApproved':False}
try:
    assert report['engine'].startswith('5.8')
    assert report['project'].replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    for name in texture_names:
        path=BASE+'/Textures/'+name
        a=unreal.load_asset(path)
        assert isinstance(a,unreal.Texture2D),'纹理不存在 '+name
        report['textures'][name]={'dimensions':[a.blueprint_get_size_x(),a.blueprint_get_size_y()],'path':path}
    for name in material_names:
        path=BASE+'/Materials/'+name
        a=unreal.load_asset(path)
        assert isinstance(a,unreal.MaterialInstanceConstant),'材质缺失 '+name
        parent=a.get_editor_property('parent')
        report['materials'][name]={'path':path,'parent':parent.get_path_name() if parent else None}
    path=BASE+'/Meshes/SM_DBA_IceBlock_FacetedCore_Review'
    mesh=unreal.load_asset(path)
    assert isinstance(mesh,unreal.StaticMesh),'冰块实体网格未能冷加载'
    report['mesh']={'path':path,'bounds':str(mesh.get_bounds()),'class':mesh.get_class().get_name()}
    for name in system_names:
        path=BASE+'/Niagara/Systems/'+name
        a=unreal.load_asset(path)
        assert isinstance(a,unreal.NiagaraSystem),'Niagara复用基础系统缺失 '+name
        info,ok,err=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(path,True)
        if not ok:raise RuntimeError('Niagara编译检测失败 '+name+' '+str(err))
        diag=json.loads(info)
        details={'path':path,'error_count':diag.get('error_count',-1),'warning_count':diag.get('warning_count',-1),
            'errors':diag.get('errors',[])[:8],'warnings':diag.get('warnings',[])[:8],
            'emitters':[v.get('emitter') for v in diag.get('emitter_stats',[])],
            'stageReadyForVisualReview':False}
        report['systems'][name]=details
    report['status']='review_baselines_cold_loaded'
    print('ORBP_V1_BASELINE_COLD_LOAD',len(report['systems']),'systems','text',len(report['textures']),
         'materials',len(report['materials']),'issues',sum(x['error_count'] for x in report['systems'].values()),flush=True)
except Exception:
    report['status']='failed';report['errors'].append(traceback.format_exc()[-5000:])
    print('ORBP_V1_COLD_LOAD_FAILED',report['errors'][-1],flush=True)
    raise
finally:
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
