# -*- coding: utf-8 -*-
"""只修复本项目花瓣绽放副本的UE5.8模块包装；保留原HLSL、输入覆盖、层序和渲染器。

通过Monolith重建三份包装并逐发射器迁移常量/链接；不操作D盘源资产，不新设计花瓣/树叶。
仅显式--apply运行，PID须匹配本轮编辑器；保存真实诊断，不将编译视为视觉验收。
"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/PetalBloomIntegration20261015'
SYSTEM='/DBAPresentationPack_Core/VFX/Library/07_Status/NS_Nature_PetalBloom_Aura'
BASE='/DBAPresentationPack_Core/VFX/Library/00_Core/NiagaraModules/Nature/'
KINDS={'NM_PetalBloom_PetalMotion':'NM_DBA_ImportedPetalMotion',
       'NM_PetalBloom_PollenPolish':'NM_DBA_ImportedPollenPolish',
       'NM_PetalBloom_PaletteParameters':'NM_DBA_ImportedPetalPalette'}
TYPES={'NiagaraInt32':'int','NiagaraFloat':'float','LinearColor':'color','Vector3f':'vec3'}


def main():
    """整批先记录原模块和输入，再替换；失败留下明确中间结果，不重放、不吞掉原生错误。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true')
    parser.add_argument('--rebind-only',action='store_true',help='替换中断后按原始证据恢复输入；不重复删除或创建模块。')
    args=parser.parse_args()
    if not args.apply:return
    owner=json.loads((EVIDENCE/'Editor.json').read_text(encoding='utf-8-sig'))
    spec=importlib.util.spec_from_file_location('petal_module_transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'PetalRepairCalls'
    p.editor_python(f"import unreal,os\nassert os.getpid()=={owner['PID']}\nassert '-PetalBloomOwnedMigration' in unreal.SystemLibrary.get_command_line()")
    def unwrap(value):return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value
    if args.rebind_only:
        # 先完成整个栈的拓扑变化，再重建链接，防止链接仍指向刚被移除模块的参数映射。
        records=json.loads((EVIDENCE/'PetalModulesBefore.json').read_text(encoding='utf8'))
        for record in records:
            target=BASE+KINDS[record['kind']]
            modules=unwrap(p.niagara('get_ordered_modules',asset_path=SYSTEM,emitter=record['emitter']))
            node=next(m['node_guid'] for m in modules if m.get('script_path','').split('.')[0]==target)
            operations=[]
            for variable in record['inputs']:
                value=record['values'][variable['name']]
                operation=dict(emitter=record['emitter'],module_node=node,input=variable['name'])
                if value.get('is_linked'):operation.update(op='set_module_input_binding',binding=value['linked_parameter'])
                else:operation.update(op='set_module_input_value',value=value['value'])
                operations.append(operation)
            p.niagara('batch_execute',asset_path=SYSTEM,operations=operations)
        result=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
        (EVIDENCE/'PetalCompileRebound.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
        print('PETAL_REBIND_RESULT',result['error_count'],result['warning_count'],result['errors'][:1],flush=True)
        if result['error_count']==0:p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
        return
    summary=p.niagara('get_system_summary',asset_path=SYSTEM)
    records=[];descriptors={}
    for emitter in summary['emitters']:
        name=emitter['name'];modules=unwrap(p.niagara('get_ordered_modules',asset_path=SYSTEM,emitter=name))
        for module in modules:
            script=module.get('script_path','').split('.')[0];kind=script.rsplit('/',1)[-1]
            if kind not in KINDS:continue
            inputs=unwrap(p.niagara('get_module_inputs',asset_path=SYSTEM,emitter=name,module_node=module['node_guid']))
            values={v['name']:p.niagara('get_module_input_value',asset_path=SYSTEM,emitter=name,module_node=module['node_guid'],input=v['name']) for v in inputs}
            records.append(dict(emitter=name,module=module,script=script,kind=kind,inputs=inputs,values=values))
            if kind not in descriptors:
                graph=p.niagara('get_module_graph',script_path=script)
                node=next(n for n in graph['nodes'] if n['class']=='NiagaraNodeCustomHlsl')
                text=p.niagara('get_custom_hlsl_text',script_path=script,node_guid=node['node_guid'])['hlsl']
                descriptors[kind]=dict(hlsl=text,inputs=[dict(name=v['name'],type=TYPES[v['type']]) for v in inputs])
    (EVIDENCE/'PetalModulesBefore.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf8')
    for kind,descriptor in descriptors.items():
        target=BASE+KINDS[kind]
        if kind!='NM_PetalBloom_PetalMotion':
            p.niagara('create_module_from_hlsl',name=KINDS[kind],save_path=target,hlsl=descriptor['hlsl'],
                inputs=descriptor['inputs'],outputs=[],description='SSLM导入兼容包装；保留原运动/颜色/花粉HLSL。')
    for record in records:
        name=record['emitter'];target=BASE+KINDS[record['kind']]
        modules=unwrap(p.niagara('get_ordered_modules',asset_path=SYSTEM,emitter=name))
        existing=[m for m in modules if m.get('script_path','').split('.')[0]==target]
        node=existing[0]['node_guid'] if existing else p.niagara('add_module',asset_path=SYSTEM,emitter=name,
            usage=record['module']['usage'],module_script=target,index=record['module']['index'])['node_guid']
        operations=[]
        for variable in record['inputs']:
            value=record['values'][variable['name']]
            if value.get('is_linked'):
                operations.append(dict(op='set_module_input_binding',emitter=name,module_node=node,input=variable['name'],binding=value['linked_parameter']))
            else:
                operations.append(dict(op='set_module_input_value',emitter=name,module_node=node,input=variable['name'],value=variable['override_value']))
        p.niagara('batch_execute',asset_path=SYSTEM,operations=operations)
        p.niagara('remove_module',asset_path=SYSTEM,emitter=name,module_node=record['module']['node_guid'])
        print('PETAL_MODULE_REBOUND',name,record['kind'],flush=True)
    result=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
    (EVIDENCE/'PetalCompile.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    assert result['error_count']==0,result['errors'][:2]
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
    p.editor_python('import unreal\n'+ '\n'.join("assert unreal.EditorAssetLibrary.save_asset('"+BASE+n+"',False)" for n in KINDS.values()))
    print('PETAL_MODULE_REPAIR_PASSED',result['error_count'],result['warning_count'],flush=True)


if __name__=='__main__':main()
