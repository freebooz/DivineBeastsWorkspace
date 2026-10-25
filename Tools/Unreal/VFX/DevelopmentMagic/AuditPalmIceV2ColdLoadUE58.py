# -*- coding: utf-8 -*-
"""《神兽联盟》冰霜双掌P1 V2 原生UE5.8冷重载核验。
从独立命令行UE进程加载真实 NiagaraSystem / MI /纹理，编译系统并核对绑定。
不得修改或保存任何Game资产，也不把编译通过等同于可见性通过。
"""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
base='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1'
stages={'01':'NS_DBA_PalmIce_P101_GatherReviewV2','02':'NS_DBA_PalmIce_P102_ChargeReviewV2'}
report={'engine':unreal.SystemLibrary.get_engine_version(),'stage':'PalmIce-P101-P102-V2','states':{},
        'status':'started','errors':[],'PIEVisualAccepted':False}
try:
    assert report['engine'].startswith('5.8')
    assert 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
    m=unreal.load_asset(base+'/Niagara/NM_DBA_PalmGatherMotion_Review')
    assert isinstance(m,unreal.NiagaraScript)
    report['sharedMotion']=m.get_path_name()
    for key,name in stages.items():
        path=base+'/Niagara/'+name
        system=unreal.load_asset(path)
        assert isinstance(system,unreal.NiagaraSystem),'V2 Niagara缺失'+path
        j,ok,err=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(path,True)
        if not ok:raise RuntimeError('Niagara diagnostics error '+str(err))
        diag=json.loads(j)
        d={'asset':path,'compileErrors':diag.get('error_count'),'compileWarnings':diag.get('warning_count'),
           'emitters':diag.get('emitter_stats',[]),'materials':[], 'motionNodeCount':0}
        for obj in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
            if obj.get_path_name().startswith(system.get_path_name()+':'):
                fn=obj.get_editor_property('function_script')
                if fn and fn==m:d['motionNodeCount']+=1
        for obj in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
            if obj.get_path_name().startswith(system.get_path_name()+':'):
                mi=obj.get_editor_property('material')
                d['materials'].append(mi.get_path_name() if mi else None)
        assert d['compileErrors']==0 and d['compileWarnings']==0,'Niagara存在真实编译错误 '+str(diag.get('errors',[]))[:1800]
        assert len(d['emitters'])==2 and all(x['sim_target']=='GPU' for x in d['emitters']), '实际模拟方式不正确 '+str(d['emitters'])
        assert d['motionNodeCount']==2,'通用旋涡模块实际引用数错误'
        assert len(d['materials'])==2 and all('/PalmElementGather/Review/V1/Materials/' in q for q in d['materials']),'材质引用异常'
        report['states'][key]=d
    report['status']='passed'
    print('PALMICE_V2_COLD_RELOAD_PASSED',len(report['states']),flush=True)
except Exception:
    report['status']='failed';report['errors'].append(traceback.format_exc()[-4000:])
    print('PALMICE_V2_COLD_RELOAD_FAILED',report['errors'][-1],flush=True)
    raise
finally:
    dest=root/'Saved/Validation/PalmIceV1/NativeColdLoadResultV2.json'
    dest.parent.mkdir(parents=True,exist_ok=True)
    dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
