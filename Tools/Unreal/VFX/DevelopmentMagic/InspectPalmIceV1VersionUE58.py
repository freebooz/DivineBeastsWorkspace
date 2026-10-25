# -*- coding: utf-8 -*-
"""SSLM冰霜审核Niagara脚本版本与函数调用校验：只读，不更改资源。"""
import unreal,json
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
B='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1'
ORIG='/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5'
MOD=B+'/Niagara/NM_DBA_PalmGatherMotion_Review'
S1=B+'/Niagara/NS_DBA_PalmIce_P101_GatherReview'
S2=B+'/Niagara/NS_DBA_PalmIce_P102_ChargeReview'
out={'scripts':{},'calls':[]}
for path in [ORIG,MOD]:
 a=unreal.load_asset(path)
 d={'path':path,'apis':[x for x in dir(a) if 'version' in x.lower() or 'input' in x.lower() or 'compile' in x.lower()]}
 for prop in ['exposed_version','version_to_open_in_editor','latest_version_guid','versioned_script_data']:
  try:d[prop]=str(a.get_editor_property(prop))
  except Exception as e:d[prop+'Error']=str(e)[:260]
 for name in ['get_exposed_version','get_all_available_versions','get_script_data','get_input_parameters']:
  if hasattr(a,name):
   try:d[name+'Result']=str(getattr(a,name)())[:500]
   except Exception as e:d[name+'Error']=str(e)[:250]
 out['scripts'][path]=d
for path in [S1,S2]:
 a=unreal.load_asset(path)
 for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
  if not n.get_path_name().startswith(a.get_path_name()+':'):continue
  f=n.get_editor_property('function_script')
  if not f or 'PalmGatherMotion' not in f.get_name():continue
  d={'node':n.get_path_name(),'script':f.get_path_name()}
  for prop in ['selected_script_version','previous_script_version','function_script_asset_object_path']:
   try:d[prop]=str(n.get_editor_property(prop))
   except Exception as e:d[prop+'Error']=str(e)[:240]
  d['methods']=[x for x in dir(n) if 'refresh' in x.lower() or 'script' in x.lower() or 'version' in x.lower()]
  out['calls'].append(d)
dest=ROOT/'Saved/Validation/PalmIceV1/NativeNiagaraVersionDiagnostics.json';dest.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PALMICE_VERSION_DIAG_WRITTEN',dest,len(out['calls']))
