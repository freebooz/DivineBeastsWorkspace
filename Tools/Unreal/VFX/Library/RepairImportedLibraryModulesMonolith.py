# -*- coding: utf-8 -*-
"""SSLM导入库原生模块兼容迁移；只复制原HLSL并保留覆盖，不设计新的效果。

默认只记录计划。显式--apply禁用导入系统的旧自定义模块并接入兼容包装；所有原输入和包字节备份
归本次Saved证据。拓扑完成后统一重绑，避免绑定引用删除节点；失败立即中止并保存诊断。
仅操作本项目内容，源D盘不编译、不保存。仅单CustomHlsl且普通数值输入的模块适用。
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/PetalBloomIntegration20261015'
BASE='/DBAPresentationPack_Core/VFX/Library/'
TYPES={'NiagaraInt32':'int','NiagaraFloat':'float','NiagaraBool':'bool','LinearColor':'color','Vector2f':'vec2','Vector3f':'vec3','Vector4f':'vec4','Quat4f':'quat','NiagaraPosition':'position','NiagaraDataInterfaceArrayPosition':'ArrayPosition'}


def main():
    """先持久化完整转换计划，再按显式选择应用，保证未支持的图不会被自动简化。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply',action='store_true');parser.add_argument('--system',default='')
    parser.add_argument('--skip-legacy',action='store_true',help='只修复完整的非Legacy效果；原型保留原始诊断。')
    parser.add_argument('--continue-on-compile-error',action='store_true',help='批次诊断继续其它独立系统；失败系统不保存，不标记可Cook。')
    parser.add_argument('--complete-complex-inputs',action='store_true',help='补录数组接口和单层动态输入；不支持的嵌套结构明确拒绝。')
    args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('library_transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'LibraryRepairCalls'
    owner=json.loads((EVIDENCE/'Editor.json').read_text(encoding='utf-8-sig'))
    p.editor_python(f"import unreal,os\nassert os.getpid()=={owner['PID']}\nassert '-PetalBloomOwnedMigration' in unreal.SystemLibrary.get_command_line()")
    def unwrap(value):return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value
    plan_file=EVIDENCE/'LibraryModulePlan.json'
    if not plan_file.exists():
        systems=json.loads((EVIDENCE/'Compile.json').read_text(encoding='utf8'))['systems']
        records=[];scripts={};unsupported=[]
        for row in systems:
            system=row['system']
            if not row['errors'] or system.endswith('NS_Nature_PetalBloom_Aura'):continue
            summary=p.niagara('get_system_summary',asset_path=system)
            for emitter in summary['emitters']:
                modules=unwrap(p.niagara('get_ordered_modules',asset_path=system,emitter=emitter['name']))
                for module in modules:
                    script=module.get('script_path','').split('.')[0]
                    if not script.startswith(BASE):continue
                    inputs=unwrap(p.niagara('get_module_inputs',asset_path=system,emitter=emitter['name'],module_node=module['node_guid']))
                    if script not in scripts:
                        graph=p.niagara('get_module_graph',script_path=script)
                        hlsl=[n for n in graph['nodes'] if n['class']=='NiagaraNodeCustomHlsl']
                        if len(hlsl)!=1 or any(v['type'] not in TYPES for v in inputs):
                            unsupported.append(dict(script=script,hlslCount=len(hlsl),inputs=inputs));scripts[script]=None
                        else:
                            body=p.niagara('get_custom_hlsl_text',script_path=script,node_guid=hlsl[0]['node_guid'])['hlsl']
                            suffix=script.rsplit('/',1)[-1]+'_'+hashlib.sha256(script.encode()).hexdigest()[:8]
                            scripts[script]=dict(hlsl=body,inputs=[dict(name=v['name'],type=TYPES[v['type']]) for v in inputs],target=BASE+'00_Core/Compatibility/'+suffix)
                    if scripts[script] is None:continue
                    values={v['name']:p.niagara('get_module_input_value',asset_path=system,emitter=emitter['name'],module_node=module['node_guid'],input=v['name']) for v in inputs}
                    if any(v.get('is_dynamic_input') for v in values.values()):
                        unsupported.append(dict(system=system,node=module['node_guid'],dynamic=True));continue
                    records.append(dict(system=system,emitter=emitter['name'],module=module,script=script,values=values))
            print('LIBRARY_MODULE_INVENTORY',system.rsplit('/',1)[-1],flush=True)
        plan_file.write_text(json.dumps(dict(records=records,scripts=scripts,unsupported=unsupported),ensure_ascii=False,indent=2),encoding='utf8')
    plan=json.loads(plan_file.read_text(encoding='utf8'))
    if args.complete_complex_inputs:
        for issue in plan['unsupported']:
            systems=[issue['system']] if issue.get('dynamic') else ['/DBAPresentationPack_Core/VFX/Library/04_Beam/Legacy/NS_Lightning_Chain_Legacy']
            for system in systems:
                # 链状闪电在原目录可能归Beam；以已盘点的真实路径为唯一真源。
                if not issue.get('dynamic'):
                    system=next(r['system'] for r in json.loads((EVIDENCE/'Compile.json').read_text(encoding='utf8'))['systems'] if r['system'].endswith('/NS_Lightning_Chain_Legacy'))
                summary=p.niagara('get_system_summary',asset_path=system)
                for emitter in summary['emitters']:
                    for module in unwrap(p.niagara('get_ordered_modules',asset_path=system,emitter=emitter['name'])):
                        script=module.get('script_path','').split('.')[0]
                        if (issue.get('dynamic') and module['node_guid']!=issue['node']) or (not issue.get('dynamic') and script!=issue['script']):continue
                        if any(r['module']['node_guid']==module['node_guid'] for r in plan['records']):continue
                        inputs=unwrap(p.niagara('get_module_inputs',asset_path=system,emitter=emitter['name'],module_node=module['node_guid']))
                        assert all(v['type'] in TYPES for v in inputs)
                        if plan['scripts'][script] is None:
                            graph=p.niagara('get_module_graph',script_path=script)
                            hlsl=[n for n in graph['nodes'] if n['class']=='NiagaraNodeCustomHlsl'];assert len(hlsl)==1
                            body=p.niagara('get_custom_hlsl_text',script_path=script,node_guid=hlsl[0]['node_guid'])['hlsl']
                            suffix=script.rsplit('/',1)[-1]+'_'+hashlib.sha256(script.encode()).hexdigest()[:8]
                            plan['scripts'][script]=dict(hlsl=body,inputs=[dict(name=v['name'],type=TYPES[v['type']]) for v in inputs],target=BASE+'00_Core/Compatibility/'+suffix)
                        values={v['name']:p.niagara('get_module_input_value',asset_path=system,emitter=emitter['name'],module_node=module['node_guid'],input=v['name']) for v in inputs}
                        tree=p.niagara('get_dynamic_input_tree',asset_path=system,emitter=emitter['name'],module_node=module['node_guid'],max_depth=3)
                        dynamic={v['name']:v['dynamic_input'] for v in tree['inputs'] if v['source']=='dynamic_input'}
                        assert all(all(i['source']=='literal' for i in d['inputs']) for d in dynamic.values()),'动态输入含绑定/嵌套，不能静默简化'
                        plan['records'].append(dict(system=system,emitter=emitter['name'],module=module,script=script,values=values,dynamic=dynamic))
        plan_file.write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf8')
    print('LIBRARY_REPAIR_PLAN',len(plan['records']),len(plan['scripts']),len(plan['unsupported']),flush=True)
    if not args.apply:return
    records=[r for r in plan['records'] if (not args.system or r['system'].endswith('/'+args.system)) and (not args.skip_legacy or '/Legacy/' not in r['system'])]
    systems=list(dict.fromkeys(r['system'] for r in records))
    result_file=EVIDENCE/'LibraryRepairResult.json'
    results=json.loads(result_file.read_text(encoding='utf8')) if result_file.exists() else []
    for system in systems:
        selected=[r for r in records if r['system']==system]
        # 目标包的原始磁盘字节只备份一次，不覆盖已有证据，不触碰外部源文件。
        asset=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'/system.removeprefix('/DBAPresentationPack_Core/')
        backup=EVIDENCE/'LibrarySystemsBefore'/asset.name
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():backup.write_bytes(asset.with_suffix('.uasset').read_bytes())
        for script in dict.fromkeys(r['script'] for r in selected):
            descriptor=plan['scripts'][script];target=descriptor['target']
            exists=p.editor_python("import unreal\nprint('COMPAT_EXISTS',unreal.EditorAssetLibrary.does_asset_exist("+repr(target)+"))")
            if 'COMPAT_EXISTS True' not in json.dumps(exists):
                # Niagara词法分析要求位运算符与输入标识分开；只补空白，不改变原始计算。
                outputs=([dict(name='RotationOut',type='float')] if script.endswith('/NM_VFX_RandomRotation') else [])
                p.niagara('create_module_from_hlsl',name=target.rsplit('/',1)[-1],save_path=target,hlsl=descriptor['hlsl'].replace('Mode&1','Mode & 1'),inputs=descriptor['inputs'],outputs=outputs,description='SSLM原库模块兼容包装；保留原始HLSL，输入与渲染由系统迁移记录恢复。')
        new_nodes=[]
        for record in selected:
            modules=unwrap(p.niagara('get_ordered_modules',asset_path=system,emitter=record['emitter']))
            target=plan['scripts'][record['script']]['target']
            existing=[m for m in modules if m.get('script_path','').split('.')[0]==target and m['usage']==record['module']['usage'] and m['enabled']]
            old=next(m for m in modules if m['node_guid']==record['module']['node_guid'])
            # Monolith0.23删除有多条参数映射链接的模块可能截断主链；禁用保留原图作迁移证据。
            p.niagara('set_module_enabled',asset_path=system,emitter=record['emitter'],module_node=old['node_guid'],enabled=False)
            node=existing[0]['node_guid'] if existing else p.niagara('add_module',asset_path=system,emitter=record['emitter'],usage=record['module']['usage'],module_script=target,index=old['index']+1)['node_guid']
            if not record['module']['enabled']:p.niagara('set_module_enabled',asset_path=system,emitter=record['emitter'],module_node=node,enabled=False)
            new_nodes.append((record,node))
        for record,node in new_nodes:
            operations=[]
            for name,value in record['values'].items():
                if value.get('is_dynamic_input'):continue
                operation=dict(emitter=record['emitter'],module_node=node,input=name)
                if value.get('is_linked'):operation.update(op='set_module_input_binding',binding=value['linked_parameter'])
                else:operation.update(op='set_module_input_value',value=value['value'])
                operations.append(operation)
            if operations:p.niagara('batch_execute',asset_path=system,operations=operations)
            for name,dynamic in record.get('dynamic',{}).items():
                created=p.niagara('add_dynamic_input',asset_path=system,emitter=record['emitter'],module_node=node,input=name,dynamic_input_script=dynamic['script_path'])
                for variable in dynamic['inputs']:
                    if 'value' in variable:p.niagara('set_dynamic_input_value',asset_path=system,emitter=record['emitter'],dynamic_input_node=created['dynamic_input_node_guid'],input=variable['name'],value=variable['value'])
        result=p.niagara('get_system_diagnostics',asset_path=system,compile_first=True)
        results=[r for r in results if r['system']!=system]+[dict(system=system,diagnostics=result)]
        (EVIDENCE/'LibraryRepairResult.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf8')
        print('LIBRARY_COMPILE_RESULT',system.rsplit('/',1)[-1],result['error_count'],result['errors'][:1],flush=True)
        if result['error_count']:
            if args.continue_on_compile_error:continue
            raise RuntimeError('原生编译仍有错误，见LibraryRepairResult.json；保留未保存中间状态供诊断')
        p.niagara('save_system',asset_path=system,only_if_dirty=False)


if __name__=='__main__':main()
