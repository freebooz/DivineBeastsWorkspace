# -*- coding: utf-8 -*-
"""修复迁入火花GPU模块遗漏的原始VelocityOut输出与尺寸曲线接口，不修改美术数值。

只操作本轮独占SSLM编辑器。原系统/原输入先保存证据，旧错误包装保留禁用；
新模块恢复vec3输出及原链接/动态输入。此修复必须再经真实GPU运行，VM通过不充分。
"""
import importlib.util
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/VFXInitialization20261016'
SYSTEM='/DBAPresentationPack_Core/VFX/Library/00_Core/Sparks/Legacy/NS_Energy_MagicSpark_Legacy'
TARGET='/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_DBA_ImportedDragWithOutput'


def repair_sprite_scaling(p):
    """恢复原生曲线接口及原始尺寸；旧作者把曲线文本写进float，并留下零初始尺寸。"""
    audit=json.loads((EVIDENCE/'InitializationInputs.json').read_text(encoding='utf8'))
    evidence=[]
    for record in audit['emitters']:
        if record['system']!=SYSTEM:continue
        size=next(v['override_value'] for v in record['initializationInputs'] if v['name']=='Sprite Size')
        module=next(v for v in record['modules'] if v['enabled'] and v['function_name']=='ScaleSpriteSize')
        args=dict(asset_path=SYSTEM,emitter=record['emitter'],module_node=module['node_guid'])
        inputs=json.loads(p.niagara('get_module_inputs',**args)['result'])
        text=next(v.get('override_value','') for v in inputs if v['name']=='Uniform Scale Factor')
        keys=[dict(time=float(t),value=float(v)) for t,v in re.findall(r'Time=([\d.]+),Value=([\d.]+)',text)]
        # 只转换已读出的原始曲线，不以默认曲线覆盖未知版本。
        if not keys:raise RuntimeError('缺少原始尺寸曲线：'+record['emitter'])
        evidence.append(dict(emitter=record['emitter'],size=size,keys=keys,before=inputs))
        p.niagara('set_module_input_value',**args,input='Initial Sprite Size',value=size)
        p.niagara('set_module_input_binding',**args,input='Uniform Curve Index',binding='Particles.NormalizedAge')
        p.niagara('configure_curve_keys',**args,input='Uniform Curve Sprite Scale',keys=keys,interp='cubic')
    (EVIDENCE/'SparkScaleRepair.json').write_text(json.dumps(evidence,ensure_ascii=False,indent=2),encoding='utf8')
    p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)


def main():
    """只重建两个GPU层的调用节点；删除旧节点存在链路风险，因此保留禁用而不截断参数映射。"""
    spec=importlib.util.spec_from_file_location('drag_transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'DragRepairCalls'
    owner=json.loads((EVIDENCE/'Editor.json').read_text(encoding='utf-8-sig'))
    p.editor_python(f"import unreal,os\nassert os.getpid()=={owner['PID']}\nassert '-VFXInitializationOwned' in unreal.SystemLibrary.get_command_line()")
    plan=json.loads((ROOT/'Saved/Validation/PetalBloomIntegration20261015/LibraryModulePlan.json').read_text(encoding='utf8'))
    key=next(k for k in plan['scripts'] if k.endswith('/NM_VFX_Drag'));descriptor=plan['scripts'][key]
    rows=[r for r in plan['records'] if r['script']==key];assert len(rows)==2 and all(r['system']==SYSTEM for r in rows)
    before=EVIDENCE/'MagicSparkBefore.uasset'
    if not before.exists():
        asset=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Library/00_Core/Sparks/Legacy/NS_Energy_MagicSpark_Legacy.uasset'
        before.write_bytes(asset.read_bytes())
    exists=p.editor_python('import unreal\nprint("DRAG_OUTPUT_EXISTS",unreal.EditorAssetLibrary.does_asset_exist('+repr(TARGET)+'))')
    if 'DRAG_OUTPUT_EXISTS True' not in json.dumps(exists):
        p.niagara('create_module_from_hlsl',name='DBAImportedDragWithOutput',save_path=TARGET,hlsl=descriptor['hlsl'],inputs=descriptor['inputs'],outputs=[dict(name='VelocityOut',type='vec3')],description='恢复原vec3 VelocityOut；公式、输入与时序保持原始资源合同。')
    for record in rows:
        emitter=record['emitter'];raw=p.niagara('get_ordered_modules',asset_path=SYSTEM,emitter=emitter);modules=json.loads(raw['result'])
        previous=[m for m in modules if m.get('script_path','').split('.')[0]==descriptor['target']]
        for module in previous:p.niagara('set_module_enabled',asset_path=SYSTEM,emitter=emitter,module_node=module['node_guid'],enabled=False)
        # 重入只复用本修复创建的同一节点，避免二次运行叠加拖曳。
        existing=next((m for m in modules if m.get('script_path','').split('.')[0]==TARGET),None)
        node=existing['node_guid'] if existing else p.niagara('add_module',asset_path=SYSTEM,emitter=emitter,usage='particle_update',module_script=TARGET,index=previous[0]['index']+1)['node_guid']
        for name,value in record['values'].items():
            if value.get('is_dynamic_input'):
                dynamic=record['dynamic'][name]
                new=p.niagara('add_dynamic_input',asset_path=SYSTEM,emitter=emitter,module_node=node,input=name,dynamic_input_script=dynamic['script_path'])['dynamic_input_node_guid']
                for v in dynamic['inputs']:
                    if 'value' in v:p.niagara('set_dynamic_input_value',asset_path=SYSTEM,emitter=emitter,dynamic_input_node=new,input=v['name'],value=v['value'])
            elif value.get('is_linked'):p.niagara('set_module_input_binding',asset_path=SYSTEM,emitter=emitter,module_node=node,input=name,binding=value['linked_parameter'])
            else:p.niagara('set_module_input_value',asset_path=SYSTEM,emitter=emitter,module_node=node,input=name,value=value['value'])
    p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
    repair_sprite_scaling(p)
    print('MAGICSPARK_DRAG_OUTPUT_SAVED',len(rows),flush=True)


if __name__=='__main__':main()
