# -*- coding: utf-8 -*-
"""SSLM项目VFX初始化审查：读取真实启用生成模块与写入，不按脚本名称消除告警。

仅连接本轮PID匹配的Monolith编辑器。保留工具原始诊断、InitializeParticle显式覆盖与
自定义HLSL；实际可见性须由同轮PIE SimCache和渲染补证。无资源写入或全库扫描。
"""
import importlib.util
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/VFXInitialization20261016'


def main():
    """只盘点上轮19条定位到的8个系统；缺少实际写入保持失败，不重命名脚本或屏蔽工具。"""
    spec=importlib.util.spec_from_file_location('init_transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'MonolithCalls'
    owner=json.loads((EVIDENCE/'Editor.json').read_text(encoding='utf-8-sig'))
    p.editor_python(f"import unreal,os\nassert os.getpid()=={owner['PID']}\nassert '-VFXInitializationOwned' in unreal.SystemLibrary.get_command_line()\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')")
    previous=json.loads((ROOT/'Saved/Validation/PetalBloomIntegration20261015/CompileReloaded.json').read_text(encoding='utf8'))
    selected=[s for s in previous['systems'] if s['warnings']]
    def unwrap(value):return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value
    result=dict(systems=[],emitters=[],scripts={},rawWarningCount=0,displayVerified=False)
    for item in selected:
        path=item['system'];diagnostic=p.niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        result['rawWarningCount']+=diagnostic['warning_count']
        summary=p.niagara('get_system_summary',asset_path=path)
        result['systems'].append(dict(system=path,diagnostics=diagnostic,summary=summary,userParameters=unwrap(p.niagara('get_user_parameters',asset_path=path))))
        for warning in diagnostic['warnings']:
            name=warning['emitter'];attribute='SpriteSize' if 'SpriteSize' in warning['message'] else 'RibbonWidth'
            modules=unwrap(p.niagara('get_ordered_modules',asset_path=path,emitter=name))
            bindings=unwrap(p.niagara('get_renderer_bindings',asset_path=path,emitter=name,renderer_index=0))
            binding=next(v['bound_to'] for v in bindings if v['name']==('SpriteSizeBinding' if attribute=='SpriteSize' else 'RibbonWidthBinding'))
            record=dict(system=path,emitter=name,attribute=attribute,boundAttribute=binding.removeprefix('Particles.'),modules=modules,proof=[])
            # 显式零生成率是作者停用该层，不得为验证凭空增加雾粒子。
            rate_nodes=[m for m in modules if m['enabled'] and m['usage']=='emitter_update' and m['function_name']=='SpawnRate']
            burst_nodes=[m for m in modules if m['enabled'] and m['usage']=='emitter_update' and 'SpawnBurst' in m['function_name']]
            if rate_nodes and not burst_nodes:
                rates=[p.niagara('get_module_input_value',asset_path=path,emitter=name,module_node=m['node_guid'],input='SpawnRate') for m in rate_nodes]
                record['explicitNoSpawn']=all(v.get('value') in ('0','0.0','0.000000') and not v.get('is_linked') and not v.get('is_dynamic_input') for v in rates)
            for module in modules:
                if module['usage']!='particle_spawn' or not module['enabled']:continue
                script=module['script_path'].split('.')[0]
                if 'InitializeParticle' in script:
                    inputs=unwrap(p.niagara('get_module_inputs',asset_path=path,emitter=name,module_node=module['node_guid']))
                    record['initializationInputs']=inputs
                    if attribute=='SpriteSize':
                        flags=[v for v in inputs if v['name']=='Write SpriteSize']
                        if flags and flags[0].get('override_value')=='true':record['proof'].append(dict(kind='NativeInitializeWrite',module=module['node_guid'],attribute='Particles.SpriteSize'))
                        # V2以真实静态枚举控制写入，不能沿用V1的Write SpriteSize开关名称。
                        if '/V2/' in script:
                            switches=p.niagara('get_static_switch_value',asset_path=path,emitter=name,module_node=module['node_guid'])['switches']
                            mode=next(v for v in switches if v['name']=='Sprite Size Mode')
                            record['spriteSizeMode']=mode
                            if mode.get('display_value') in ('Uniform','Random Uniform','Non-Uniform','Random Non-Uniform'):
                                record['proof'].append(dict(kind='NativeV2SizeMode',module=module['node_guid'],attribute='Particles.SpriteSize',mode=mode['display_value']))
                if not script.startswith('/DBAPresentationPack_Core/'):continue
                if script not in result['scripts']:
                    graph=p.niagara('get_module_graph',script_path=script)
                    nodes=[n for n in graph['nodes'] if n['class']=='NiagaraNodeCustomHlsl']
                    result['scripts'][script]='\n'.join(p.niagara('get_custom_hlsl_text',script_path=script,node_guid=n['node_guid'])['hlsl'] for n in nodes)
                hlsl=result['scripts'][script]
                if re.search(r'\bParticles\.'+attribute+r'\s*=(?!=)',hlsl):record['proof'].append(dict(kind='CustomHlslSpawnWrite',module=module['node_guid'],script=script,attribute='Particles.'+attribute))
            record['hasSpawnWrite']=bool(record['proof']);result['emitters'].append(record)
    (EVIDENCE/'InitializationInputs.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print('INITIALIZATION_AUDIT',len(result['systems']),len(result['emitters']),'raw',result['rawWarningCount'],'missing',[(e['emitter'],e['attribute']) for e in result['emitters'] if not e['hasSpawnWrite']],flush=True)


if __name__=='__main__':main()
