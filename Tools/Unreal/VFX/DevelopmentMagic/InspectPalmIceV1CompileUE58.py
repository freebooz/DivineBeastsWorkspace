# -*- coding:utf-8 -*-
"""冰霜聚能Niagara模块原生只读诊断：HLSL、输入引脚和调用节点签名。"""
import unreal,json
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
base='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1'
paths={'original':'/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5','review':base+'/Niagara/NM_DBA_PalmGatherMotion_Review','system01':base+'/Niagara/NS_DBA_PalmIce_P101_GatherReview','system02':base+'/Niagara/NS_DBA_PalmIce_P102_ChargeReview'}
out={}
for key,path in paths.items():
 a=unreal.load_asset(path); item={'class':a.get_class().get_name() if a else 'notfound','hlsl':[],'inputs':[],'calls':[]}
 if a:
  for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl):
   if n.get_path_name().startswith(a.get_path_name()+':'):item['hlsl'].append(str(n.get_editor_property('custom_hlsl')))
  for n in unreal.ObjectIterator(unreal.NiagaraNodeInput):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    d={'path':n.get_path_name()}
    for prop in ['input','usage','exposure_options']:
     try:d[prop]=str(n.get_editor_property(prop))
     except:pass
    d['apis']=[x for x in ['get_input_pins','get_output_pins','get_pins','refresh_from_external_changes','reconstruct_node'] if hasattr(n,x)]
    item['inputs'].append(d)
  for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
   if n.get_path_name().startswith(a.get_path_name()+':'):
    f=n.get_editor_property('function_script')
    item['calls'].append({'path':n.get_path_name(),'script':f.get_path_name() if f else None,'apis':[x for x in ['get_input_pins','get_output_pins','get_pins','refresh_from_external_changes','reconstruct_node'] if hasattr(n,x)]})
 out[key]=item
dest=root/'Saved/Validation/PalmIceV1/NativeNiagaraNodeDiagnostics.json';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('PALMICE_MODULE_INSPECTED',str(dest),list(out.keys()))
