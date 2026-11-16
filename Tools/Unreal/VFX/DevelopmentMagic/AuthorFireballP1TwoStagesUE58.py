# -*- coding: utf-8 -*-
"""SSLM《神兽联盟》火球V3 P1：创建01双手初燃、02双掌蓄力的独立审核Niagara及材质实例。

真实UE5.8资产作者，不触碰正式VFX、服务器GAS和现有地图/人物。
仅复制公共FireMage已有CPU Niagara与现有材质实例，替换审核副本中的Sprite Renderer Material。
四份材质实例 + 两份Niagara审核系统；所有目标路径严格不存在才执行。
失败时保留已生成的审核资源进行故障核查，不删除、不覆盖。
"""
import unreal, json, traceback, hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
LIB='/DBAPresentationPack_Core/VFX/Library/Game/FireMage'
BASE='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands'
TEMPLATE=LIB+'/Niagara/NS_PY_Charge'
CONF=[
    {"id":"01","semantic":"HandsIgnite","label":"双手初燃",
     "core_energy":1.65,"core_density":0.54,"ember_energy":2.7,"ember_density":0.38,
     "desiredVisualDiameterCm":16,"reviewSystem":BASE+'/Niagara/NS_DBA_Fireball_P1_01_HandsIgnite_Review'},
    {"id":"02","semantic":"PalmsCharge","label":"双掌蓄力",
     "core_energy":2.4,"core_density":0.78,"ember_energy":4.2,"ember_density":0.65,
     "desiredVisualDiameterCm":28,"reviewSystem":BASE+'/Niagara/NS_DBA_Fireball_P1_02_PalmsCharge_Review'}
]
OUT=ROOT/'Saved/Validation/FireballV3P1/AuthorHandsIgniteChargeReviewUE58.json'
result={"project":unreal.Paths.get_project_file_path(),"engine":unreal.SystemLibrary.get_engine_version(),
 "scope":"P1 01-02 independent review assets only","states":[],"state":"preflight","changes":[],"error":None,
 "originalResourcesModified":False,"serverTimingModified":False,"manualAccepted":False,"nativeVisualAccepted":False}
try:
    assert unreal.SystemLibrary.get_engine_version().startswith("5.8")
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    sources={
      "system":TEMPLATE,
      "core":LIB+'/Materials/Instances/MI_PY_FlipbookCore',
      "ember":LIB+'/Materials/Instances/MI_PY_Ember',
      "flipbook":LIB+'/Textures/T_PY_FlameFlipbook'
    }
    for k,p in sources.items():
        asset=unreal.load_asset(p)
        if not asset:raise RuntimeError('缺少复用源资源 '+k+' '+p)
    targets=[]
    for cfg in CONF:
        pre=cfg['id']
        cfg['coreMI']=BASE+'/Materials/MI_DBA_Fireball_P1_'+pre+'_Core_Review'
        cfg['emberMI']=BASE+'/Materials/MI_DBA_Fireball_P1_'+pre+'_Ember_Review'
        targets.extend([cfg['coreMI'],cfg['emberMI'],cfg['reviewSystem']])
    for p in targets:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            raise RuntimeError('P1独立审核路径已存在，严禁覆盖 '+p)
    result['state']='authoring'
    for cfg in CONF:
        row={'stage':cfg['id'],'label':cfg['label'],'state':'authoring','assets':[],'coreEnergy':cfg['core_energy'],
             'coreDensity':cfg['core_density'],'emberEnergy':cfg['ember_energy'],'emberDensity':cfg['ember_density']}
        result['states'].append(row)
        core=unreal.EditorAssetLibrary.duplicate_asset(sources['core'],cfg['coreMI'])
        ember=unreal.EditorAssetLibrary.duplicate_asset(sources['ember'],cfg['emberMI'])
        assert isinstance(core,unreal.MaterialInstanceConstant) and isinstance(ember,unreal.MaterialInstanceConstant)
        row['assets'].extend([cfg['coreMI'],cfg['emberMI']])
        set_scalar=unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value
        for name,asset,param in [('core',core,cfg),('ember',ember,cfg)]:
            if name=='core':
                set_scalar(asset,'Energy',cfg['core_energy'])
                set_scalar(asset,'Density',cfg['core_density'])
            else:
                set_scalar(asset,'Energy',cfg['ember_energy'])
                set_scalar(asset,'Density',cfg['ember_density'])
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset,False):
                raise RuntimeError('材质实例保存失败 '+name+' '+cfg['id'])
        ns=unreal.EditorAssetLibrary.duplicate_asset(TEMPLATE,cfg['reviewSystem'])
        assert isinstance(ns,unreal.NiagaraSystem)
        row['assets'].append(cfg['reviewSystem'])
        # UE5.8通过该审核Niagara的子对象原生Renderer对象编辑材质，不重新创建粒子框架。
        nodes=[r for r in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties)
               if r.get_path_name().startswith(ns.get_path_name()+':')]
        if len(nodes)!=2:raise RuntimeError('审核Niagara渲染器必须恰好2个，实际 '+str(len(nodes)))
        for renderer in nodes:
            path=renderer.get_path_name()
            mat=core if ':HandFlame_' in path else ember if ':GatheringSparks_' in path else None
            if mat is None:raise RuntimeError('无法定位原生Niagara发射器：'+path)
            renderer.set_editor_property('material',mat)
            if renderer.get_editor_property('material')!=mat:
                raise RuntimeError('渲染器材质设置后回读失败：'+path)
            row.setdefault('renderers',[]).append({'object':path,'material':mat.get_path_name()})
        if not unreal.EditorAssetLibrary.save_loaded_asset(ns,False):
            raise RuntimeError('审核Niagara系统保存失败：'+cfg['reviewSystem'])
        for p in row['assets']:
            ref=unreal.load_asset(p)
            if not ref:raise RuntimeError('审核资产保存后不存在 '+p)
        row['state']='created'
        result['changes'].extend(row['assets'])
    result['state']='created'
    result['texture']={'reused':sources['flipbook'],'originalTextureSize':[1254,1254],
        'authoritativeTextureSource':'项目原有FireMage火焰图集，待审核批准后再考虑1024方形图集重采样'}
    print('SSLM_FIREBALL_P1_HANDS_01_02_NATIVE_ASSETS_CREATED',len(result['changes']))
except Exception as e:
    result['state']='failed'
    result['error']=str(e)
    result['traceback']=traceback.format_exc()[-6000:]
    print('SSLM_FIREBALL_P1_HANDS_AUTHOR_FAILED',str(e))
    raise
finally:
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
