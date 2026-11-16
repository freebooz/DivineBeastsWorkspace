# -*- coding:utf-8 -*-
"""只读检查UE5.8 Niagara 原生Python作者能力与MonolithSettings反射属性。"""
import unreal,json,traceback,os
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
out={'engine':unreal.SystemLibrary.get_engine_version(),'pluginSettings':{},'authorAPIs':{},'runtimeClasses':[]}
for name in ['NiagaraEditorLibrary','NiagaraSystemFactoryNew','NiagaraEmitterFactoryNew','NiagaraScriptFactoryNew','NiagaraEditorSettings','MonolithSettings','MonolithNiagaraQueryLibrary','NiagaraNodeFunctionCall','NiagaraEmitter']:
  typ=getattr(unreal,name,None)
  if typ is None:
    try:typ=unreal.load_class(None,'/Script/MonolithCore.'+name)
    except Exception:pass
  x={'bound':bool(typ),'pythonType':str(typ)}
  if typ:
    x['functions']=[n for n in dir(typ) if not n.startswith('__') and any(k in n.lower() for k in ['emitter','module','renderer','script','system','parameter','port','graph','create','property'])]
    if name=='MonolithSettings':
      try:
        default=unreal.get_default_object(typ)
        x['actualPythonType']=str(type(default))
        x['class']=default.get_class().get_path_name()
        for attr in ['server_port','ServerPort','serverPort','b_mcp_server_enabled','bMcpServerEnabled']:
          try:x[attr]=str(default.get_editor_property(attr))
          except Exception as ex:x[attr+'Error']=str(ex)[:350]
        x['dirMatches']=[a for a in dir(default) if 'port' in a.lower() or 'server' in a.lower()]
      except Exception as ex:x['defaultError']=str(ex)[:350]
  out['authorAPIs'][name]=x
dest=root/'Saved/Validation/OrbProjectileV1/UEPythonAPIs.json'
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('ORBP_UE_API_PROBE',str(dest))
