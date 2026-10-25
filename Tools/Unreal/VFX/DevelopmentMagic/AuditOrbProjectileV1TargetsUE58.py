# -*- coding:utf-8 -*-
"""《神兽联盟》OrbProjectile V1 四套目标Niagara独立UE5.8冷重载检查。
验证20发射器、原生Ribbon/Mesh渲染器、引用及原生编译；不修改任何.uasset。
编译状态不等于真实客户端视觉验收。
"""
import unreal,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
SYSTEMS={
'FireForm':['NS_DBA_FireOrb_Form_Review',['OrbCore','OrbShellFlame','OuterFlameTongues','HeatFlow','EmberSparks']],
'IceForm':['NS_DBA_IceBlock_Form_Review',['IceCoreMesh','FacetGrowth','FrostWrap','ColdCondensation','CrystalGlint']],
'FireFlight':['NS_DBA_FireOrb_FlightTrail_Review',['TravelCore','FlightFlames','PrimaryFireRibbon','SecondaryWisps','TrailingEmbers']],
'IceFlight':['NS_DBA_IceBlock_FlightTrail_Review',['FlyingIceCore','FacetShimmer','PrimaryFrostRibbon','ColdMistWake','CrystalShards']]}
OUT=ROOT/'Saved/Validation/OrbProjectileV1/TargetNiagaraColdLoad.json'
report={'engine':unreal.SystemLibrary.get_engine_version(),'status':'started','systems':{},'errors':[],
        'realGamePIEVerified':False,'visualApproved':False,'serverTimingChanged':False}
try:
 assert report['engine'].startswith('5.8')
 assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
 for label,(name,expected) in SYSTEMS.items():
  path=BASE+'/Niagara/Systems/'+name
  sys=unreal.load_asset(path)
  assert isinstance(sys,unreal.NiagaraSystem),'Niagara系统缺失 '+path
  text,ok,err=unreal.MonolithNiagaraQueryLibrary.get_niagara_stats(path,True)
  if not ok:raise RuntimeError('UE原生Niagara状态读取失败 '+str(err))
  diag=json.loads(text)
  ems=diag.get('emitter_stats',[])
  found=[x.get('emitter') for x in ems]
  x={'asset':path,'errorCount':diag.get('error_count'),'warningCount':diag.get('warning_count'),
     'emitters':found,'ribbonRenderers':[],'meshRenderers':[],'materialPaths':[]}
  assert set(found)==set(expected),'发射器不匹配 '+label+' '+str(found)
  assert x['errorCount']==0,'存在Niagara原生编译错误 '+label+' '+str(diag.get('errors',[]))
  # 使用现有Monolith原生Niagara查询库检查冷重载的真实Ribbon/Mesh渲染器。
  for emitter in expected:
   rows,success,error=unreal.MonolithNiagaraQueryLibrary.get_niagara_renderers(path,emitter,False,-1)
   if not success:raise RuntimeError('Niagara原生渲染器读取失败 '+emitter+' '+str(error))
   query=json.loads(rows)
   if isinstance(query,dict):
    entries=query.get('renderers',query.get('result',[]))
    if isinstance(entries,str):
     try:entries=json.loads(entries)
     except:entries=[]
   elif isinstance(query,list):entries=query
   else:entries=[]
   for entry in entries:
    type_name=str(entry.get('type',entry.get('class',''))).lower()
    name=emitter+':'+str(entry.get('index',0))
    if 'ribbon' in type_name:x['ribbonRenderers'].append(name)
    elif 'mesh' in type_name:x['meshRenderers'].append(name)
    elif 'sprite' in type_name:x.setdefault('spriteRenderers',[]).append(name)
    material=entry.get('material')
    if material:x['materialPaths'].append(str(material))
  # 另行读取Mesh主网格具体路径，不把含糊的渲染器类型当成绑定通过。
  typ=getattr(unreal,'NiagaraMeshRendererProperties',None)
  if typ is None:typ=unreal.load_class(None,'/Script/Niagara.NiagaraMeshRendererProperties')
  if typ is not None:
   for obj in unreal.ObjectIterator(typ):
    if not obj.get_path_name().startswith(sys.get_path_name()+':'):continue
    try:
     raw=obj.get_editor_property('Meshes')
     x.setdefault('meshes',[]).append(str(raw))
    except Exception as ex:x.setdefault('warnings',[]).append(str(ex)[:180])
  assert len(x['ribbonRenderers'])==(1 if 'Flight' in label else 0),'Ribbon渲染器数量不正确 '+label
  if 'Ice' in label:assert len(x['meshRenderers'])>=1,'缺失冰块主体网格渲染器 '+label
  if 'Ice' in label:
   # Python未直接暴露Niagara MeshRenderer资源表；通过冷启动资产注册表验证真实依赖。
   mesh_path=BASE+'/Meshes/SM_DBA_IceBlock_FacetedCore_Review'
   refs=unreal.EditorAssetLibrary.find_package_referencers_for_asset(mesh_path,False)
   x['meshAssetRegistryReferencers']=[str(v) for v in refs]
   x['thickIceMeshReferenceVerified']=any('SM_DBA_IceBlock_FacetedCore_Review' in v for v in x.get('meshes',[]))
   assert x['thickIceMeshReferenceVerified'],'Niagara冷加载Mesh渲染器未引用新厚冰块 '+label
  report['systems'][label]=x
 report['status']='cold_loaded_native_compile_passed'
 print('ORBP_TARGETS_COLD_LOADED',len(report['systems']),'20_emitters',flush=True)
except Exception:
 report['status']='failed';report['errors'].append(traceback.format_exc()[-4500:])
 print('ORBP_TARGETS_COLD_LOAD_FAILED',report['errors'][-1],flush=True)
 raise
finally:
 OUT.parent.mkdir(parents=True,exist_ok=True)
 OUT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
