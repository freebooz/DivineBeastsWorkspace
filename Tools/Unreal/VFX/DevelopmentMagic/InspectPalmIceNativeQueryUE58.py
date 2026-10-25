# -*- coding: utf-8 -*-
"""不依赖HTTP端口，通过原生UE5.8内嵌Monolith蓝图查询库诊断冰霜审核Niagara编译；严格只读。"""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
base='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1/Niagara/'
out={'niagaraLibraryAvailable':hasattr(unreal,'MonolithNiagaraQueryLibrary'),'entries':{}}
if hasattr(unreal,'MonolithNiagaraQueryLibrary'):
 cls=unreal.MonolithNiagaraQueryLibrary
 out['methods']=[v for v in dir(cls) if ('niagara' in v.lower() or 'stat' in v.lower())]
 for stem in ['NS_DBA_PalmIce_P101_GatherReview','NS_DBA_PalmIce_P102_ChargeReview']:
  path=base+stem
  d={'path':path}
  try:
   v=cls.get_niagara_stats(path,True)
   d['return']=str(v)[:12500]
  except Exception as e:d['error']=repr(e)
  out['entries'][stem]=d
dest=root/'Saved/Validation/PalmIceV1/NativeNiagaraQueryResult.json'
dest.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PALMICE_QUERY',str(dest))
