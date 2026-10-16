#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM冰箭原节点修复与保存重载门禁，只经本任务独占的Monolith编辑器操作。

原有模块隐藏ParameterMap输入，重编后系统调用的InputMap变成孤立引脚；本工具只公开
原输入、刷新同一FunctionCall并恢复原参数绑定，不重建系统、不改资源身份或玩法。
每次请求/响应存入独占Saved证据。默认只读；--repair-stage修改指定阶段；--verify-reload
只从已保存磁盘重载六系统并强制编译验收，不启动PIE、不编译C++或Cook。
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re

from AuthorIceArrowNiagara import DEST, MODULE, SYSTEMS


ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = ROOT / 'Saved/Validation/WorldEntryFix20261015/IceArrowNativeGraphRepair'


def protocol(expected_editor_pid):
    """只连接当前9316服务，校验正式项目/锁定版本和任务所有权PID，失败不换实例重试。"""
    spec = importlib.util.spec_from_file_location('ice_repair_protocol',
        ROOT / 'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(p)
    p.EVIDENCE = EVIDENCE
    if expected_editor_pid <= 0:
        raise ValueError('必须明确指定已核实所有权的编辑器PID')
    p.editor_python("import unreal,os\nassert os.getpid()==" + str(expected_editor_pid) + ", '冰箭修复编辑器所有权已变化'\n"
        "assert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\n"
        "assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\nprint('ICE_REPAIR_IDENTITY_OK')")
    return p


def native_contract(p, paths, refresh=False):
    """读取唯一模块输入暴露与原调用身份；写入只在显式refresh时触发原生引脚重配。"""
    command = '''import unreal,json
module=''' + repr(MODULE) + '''
script=unreal.EditorAssetLibrary.load_asset(module)
inputs=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith(module+'.')]
assert len(inputs)==1,'模块ParameterMap输入必须唯一'
node=inputs[0];options=node.get_editor_property('exposure_options')
if ''' + repr(refresh) + ''':
 options.set_editor_property('exposed',True)
 options.set_editor_property('required',True)
 options.set_editor_property('hidden',False)
 node.set_editor_property('exposure_options',options)
assert options.get_editor_property('exposed') and options.get_editor_property('required') and not options.get_editor_property('hidden'),options.export_text()
rows={}
for path in ''' + repr(paths) + ''':
 unreal.EditorAssetLibrary.load_asset(path)
 calls=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall) if n.get_path_name().startswith(path+'.') and n.get_editor_property('function_script') and n.get_editor_property('function_script').get_path_name().split('.')[0]==module]
 rows[path]=[n.get_path_name() for n in calls]
 for n in calls:
  if ''' + repr(refresh) + ''':
   n.set_editor_property('function_script',None)
   n.set_editor_property('function_script',script)
if ''' + repr(refresh) + ''':
 assert unreal.EditorAssetLibrary.save_loaded_asset(script,False)
print('ICE_NATIVE_CONTRACT '+json.dumps({'exposure':options.export_text(),'calls':rows}))
'''
    response = p.editor_python(command)
    for output in response.get('output', []):
        for line in output.get('output', '').splitlines():
            if line.startswith('ICE_NATIVE_CONTRACT '):
                return json.loads(line.removeprefix('ICE_NATIVE_CONTRACT '))
    raise RuntimeError('Monolith未返回原生模块/调用合同')


def unpack(value):
    """兼容Monolith数组结果的明确JSON包装，不把文本成功当成图数据。"""
    return json.loads(value['result']) if isinstance(value, dict) and isinstance(value.get('result'), str) else value


def validate_emitter_graph(graph):
    """真实nodes/edges门禁：两个原调用各有唯一InputMap，三处动态读取均来自调用上游。

    核验循环与绑定来源，避免函数刷新或重新绑定制造下游反馈；此检查不能替代编译和重载。
    """
    nodes = {node['node_guid']: node for node in graph['nodes']}
    edges = graph['edges']
    calls = [node for node in nodes.values() if node.get('function_script', '').split('.')[0] == MODULE]
    assert len(calls) == 2, '每个发射器必须保留生成和更新两次原调用'
    adjacency = {key: [] for key in nodes}
    for edge in edges:
        adjacency[edge['from_node_guid']].append(edge['to_node_guid'])
    visiting, visited = set(), set()
    def visit(key):
        assert key not in visiting, '冰箭参数图出现回环'
        if key in visited:
            return
        visiting.add(key)
        for target in adjacency[key]:
            visit(target)
        visiting.remove(key)
        visited.add(key)
    for key in nodes:
        visit(key)
    bindings = 0
    for call in calls:
        name = call['function_name']
        links = [edge for edge in edges if edge['to_node_guid'] == call['node_guid'] and edge['to_pin_name'] == 'InputMap']
        assert len(links) == 1, '每个原调用必须有唯一InputMap上游'
        override = links[0]['from_node_guid']
        upstream = [edge for edge in edges if edge['to_node_guid'] == override and edge['to_pin_name'] == 'Source']
        assert len(upstream) == 1, '覆盖节点必须来自唯一上游'
        origin = (upstream[0]['from_node_guid'], upstream[0]['from_pin_name'])
        # 更新多一项Age动态绑定，生成固定0；不接受按名字猜测当前事实。
        expected = {'Index': 'Particles.UniqueID'}
        age = [edge for edge in edges if edge['to_node_guid'] == override and edge['to_pin_name'] == name + '.Age']
        if age:
            expected['Age'] = 'Particles.Age'
        for input_name, binding in expected.items():
            link = [edge for edge in edges if edge['to_node_guid'] == override and edge['to_pin_name'] == name + '.' + input_name]
            assert len(link) == 1 and link[0]['from_pin_name'] == binding, '冰箭动态输入错绑：' + input_name
            reader = link[0]['from_node_guid']
            source = [edge for edge in edges if edge['to_node_guid'] == reader and edge['to_pin_name'] == 'Source']
            assert len(source) == 1 and (source[0]['from_node_guid'], source[0]['from_pin_name']) == origin, '冰箭输入读取下游：' + input_name
            bindings += 1
    assert bindings == 3, '生成Index+更新Age/Index必须完整'
    return {'callCount': len(calls), 'dynamicBindings': bindings, 'acyclic': True}


def repair_stage(p, stage):
    """只刷新指定阶段的同一调用；按GUID恢复原输入，强制编译成功才保存该系统。"""
    path = DEST + '/Niagara/NS_DBA_IceArrow_' + stage
    before = native_contract(p, [path], refresh=True)
    assert len(before['calls'][path]) == len(SYSTEMS[stage]) * 2
    graphs, operations = {}, []
    for emitter, mode, count, life, material, mesh in SYSTEMS[stage]:
        modules = unpack(p.niagara('get_ordered_modules', asset_path=path, emitter=emitter))
        motion = [node for node in modules if node.get('script_path', '').split('.')[0] == MODULE]
        assert {node['usage'] for node in motion} == {'particle_spawn', 'particle_update'} and len(motion) == 2
        for node in motion:
            common = dict(asset_path=path, emitter=emitter, module_node=node['node_guid'])
            operations.append(dict(op='set_module_input_binding', **common, input='Index', binding='Particles.UniqueID'))
            if node['usage'] == 'particle_update':
                operations.append(dict(op='set_module_input_binding', **common, input='Age', binding='Particles.Age'))
            else:
                operations.append(dict(op='set_module_input_value', **common, input='Age', value='0'))
            operations.append(dict(op='set_module_input_value', **common, input='Mode', value=str(mode)))
            operations.append(dict(op='set_module_input_value', **common, input='Life', value=str(life)))
    batch = p.niagara('batch_execute', asset_path=path, operations=operations)
    assert batch.get('failed', 0) == 0 and all(row.get('success', False) for row in batch['results']), batch
    for emitter, mode, count, life, material, mesh in SYSTEMS[stage]:
        graph_path = path + '.' + path.rsplit('/', 1)[1] + ':' + emitter + '_0.UpdateScript'
        graph = p.niagara('get_module_graph', script_path=graph_path, links=True)
        graphs[emitter] = validate_emitter_graph(graph)
    p.niagara('request_compile', asset_path=path, force=True, synchronous=True)
    diagnostic = p.niagara('get_system_diagnostics', asset_path=path, compile_first=False)
    assert diagnostic['error_count'] == 0 and diagnostic['warning_count'] == 0, diagnostic
    assert all(item['compile_status'] == 'UpToDate' for emitter in diagnostic['emitter_stats'] for item in emitter['scripts'])
    p.niagara('save_system', asset_path=path, only_if_dirty=False)
    after = native_contract(p, [path])
    assert before['calls'] == after['calls'], '原调用对象身份意外变化'
    result = {'stage': stage, 'nativeContract': after, 'graphs': graphs, 'diagnostics': diagnostic,
              'saved': True, 'reloadVerified': False, 'cookVerified': False, 'manualVisualAccepted': False}
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    (EVIDENCE / (stage + '_Repaired.json')).write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print('ICE_STAGE_REPAIRED', stage, 'emitters', len(graphs), flush=True)


def verify_reload(p):
    """原生重载七包后重新强编译六系统，逐发射器记录实际译后GPU输入，不宣称画面通过。"""
    paths = [MODULE] + [DEST + '/Niagara/NS_DBA_IceArrow_' + stage for stage in SYSTEMS]
    p.editor_python('''import unreal
paths=''' + repr(paths) + '''
packages=[unreal.EditorAssetLibrary.load_asset(path).get_outermost() for path in paths]
reloaded,error=unreal.EditorLoadingAndSavingUtils.reload_packages(packages,interaction_mode=unreal.ReloadPackagesInteractionMode.ASSUME_NEGATIVE)
assert reloaded and not str(error),(reloaded,str(error))
print('ICE_NATIVE_RELOADED',len(packages))
''')
    native = native_contract(p, paths[1:])
    results = {}
    for stage, emitters in SYSTEMS.items():
        path = DEST + '/Niagara/NS_DBA_IceArrow_' + stage
        p.niagara('request_compile', asset_path=path, force=True, synchronous=True)
        diagnostic = p.niagara('get_system_diagnostics', asset_path=path, compile_first=False)
        assert diagnostic['error_count'] == 0 and diagnostic['warning_count'] == 0, diagnostic
        assert all(item['compile_status'] == 'UpToDate' for emitter in diagnostic['emitter_stats'] for item in emitter['scripts'])
        hlsl_rows = {}
        for emitter, mode, count, life, material, mesh in emitters:
            response = p.niagara('get_compiled_gpu_hlsl', asset_path=path, emitter=emitter)
            hlsl = response.get('result', '')
            assert re.search(r'int i\s*=\s*In_Index\s*%\s*128', hlsl), '译后GPU仍缺少Index输入替换：' + emitter
            assert not re.search(r'int i\s*=\s*Index\s*%', hlsl), '译后GPU保留未声明Index：' + emitter
            hlsl_rows[emitter] = {'inputReplaced': True, 'length': len(hlsl),
                'writesSpawnMap': 'Context.MapSpawn.Particles.Color=float4(tint,opacity)' in hlsl,
                'writesUpdateMap': 'Context.MapUpdate.Particles.Color=float4(tint,opacity)' in hlsl}
            assert hlsl_rows[emitter]['writesUpdateMap'] and not hlsl_rows[emitter]['writesSpawnMap'], '副作用HLSL仍写入Spawn缓存而非当前Update：' + emitter
        p.niagara('save_system', asset_path=path, only_if_dirty=False)
        results[stage] = {'diagnostics': diagnostic, 'gpuHlsl': hlsl_rows}
        print('ICE_RELOAD_COMPILED', stage, 'GPU', len(hlsl_rows), flush=True)
    payload = {'nativeReloadVerified': True, 'inputReplacementVerified': True, 'nativeContract': native,
               'systems': results, 'cookVerified': False, 'manualVisualAccepted': False}
    (EVIDENCE / 'ReloadAndGPUHlsl.json').write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding='utf-8')


def apply_update_only(p):
    """禁用原重复Spawn调用并保留其节点，唯一Update负责全部视觉属性，避免跨阶段正文缓存。

    不关闭引擎插值生成：实际GPU译文已证明其仍对新粒子运行Update，因此无需扩大修改范围。
    原Update节点及绑定/参数/原生寿命、材质、网格均保持；每阶段编译后保存，不重建资产。
    """
    for stage, emitters in SYSTEMS.items():
        path = DEST + '/Niagara/NS_DBA_IceArrow_' + stage
        operations = []
        for emitter, mode, count, life, material, mesh in emitters:
            modules = unpack(p.niagara('get_ordered_modules', asset_path=path, emitter=emitter))
            motion = [node for node in modules if node.get('script_path', '').split('.')[0] == MODULE]
            assert len(motion) == 2 and {node['usage'] for node in motion} == {'particle_spawn', 'particle_update'}
            spawn = next(node for node in motion if node['usage'] == 'particle_spawn')
            update = next(node for node in motion if node['usage'] == 'particle_update')
            assert update['enabled'], '原Update调用必须存在且启用'
            operations.append(dict(op='set_module_enabled', emitter=emitter,
                module_node=spawn['node_guid'], enabled=False))
        batch = p.niagara('batch_execute', asset_path=path, operations=operations)
        assert batch['failed'] == 0 and batch['succeeded'] == len(emitters), batch
        p.niagara('request_compile', asset_path=path, force=True, synchronous=True)
        diagnostic = p.niagara('get_system_diagnostics', asset_path=path, compile_first=False)
        assert diagnostic['error_count'] == 0 and diagnostic['warning_count'] == 0, diagnostic
        p.niagara('save_system', asset_path=path, only_if_dirty=False)
        print('ICE_UPDATE_ONLY_SAVED', stage, len(emitters), flush=True)


def main():
    """显式阶段写入开关，其余默认只读；任何断言/工具失败非零退出，禁止继续后继阶段。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor-pid', type=int, required=True, help='刚核实拥有正式工程的编辑器进程，必须与Monolith实际进程一致')
    parser.add_argument('--repair-stage', choices=tuple(SYSTEMS))
    parser.add_argument('--repair-all', action='store_true', help='串行修复全部六阶段，不并发修改同一模块')
    parser.add_argument('--verify-reload', action='store_true', help='从磁盘重载已保存七包并强制编译及核对GPU译后输入')
    parser.add_argument('--update-only', action='store_true', help='仅禁用原重复Spawn视觉模块，保留原Update及全部资源身份')
    args = parser.parse_args()
    p = protocol(args.editor_pid)
    if args.update_only:
        apply_update_only(p)
    elif args.verify_reload:
        verify_reload(p)
    elif args.repair_all:
        for stage in SYSTEMS:
            repair_stage(p, stage)
    elif args.repair_stage:
        repair_stage(p, args.repair_stage)
    else:
        paths = [DEST + '/Niagara/NS_DBA_IceArrow_' + stage for stage in SYSTEMS]
        print(json.dumps(native_contract(p, paths), ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
