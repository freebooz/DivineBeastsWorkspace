#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM A 方案逐层作者：只通过已发现的 Monolith 接口编辑 Review 候选。

复用既有 MCP 协议、模板和母材质。每次调用只增加明确指定的一层，记录每个请求，
编译及配置回读通过后保存；真实 GPU 着色器与截图必须由后续审核确认。
不修改已发布 Definition、目录映射或服务器判定；已有同名发射器拒绝覆盖。
"""
from pathlib import Path
import argparse
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[4]
BASE = '/DBAPresentationPack_Core/VFX/IceArrow'
SYSTEM = BASE + '/Niagara/Review/NS_DBA_IceArrow_Flight_A'
MODULE = BASE + '/Niagara/Review/NM_DBA_IceArrowMotion_A'
LAYERS = {
    'FacetPlates': (16, 5, 3., 'Review/MI_DBA_IceArrow_Facet_A', 'SM_DBA_IceArrow_CrystalShard'),
    'FrostFlow': (11, 2, 3., 'Review/MI_DBA_IceArrow_Flow_A', 'SM_DBA_IceArrow_TrailRibbon'),
    'FrostShards': (13, 7, 3., 'Review/MI_DBA_IceArrow_Crystal_A', 'SM_DBA_IceArrow_CrystalShard'),
}


def protocol():
    """校验所属项目和显式启动标记；失败不改端口、不重放修改。"""
    spec = importlib.util.spec_from_file_location('ice_a_protocol', ROOT / 'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(p)
    p.EVIDENCE = ROOT / 'Saved/Validation/IceArrow/Production_20261016/LayerOperations'
    p.editor_python("import unreal\nassert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')")
    return p


def add_mesh_layer(p, system, name, mode, count, life, material, mesh):
    """逐层装配；mesh为空沿用Sprite，绝对路径可复用引擎网格。位置厘米、寿命秒。"""
    before = p.niagara('get_system_summary', asset_path=system, detail_level='compact')
    if name in [e['name'] for e in before['emitters']]:
        raise RuntimeError('发射器已存在，先检查当前状态而非重复创建：' + name)
    p.niagara('add_emitter', asset_path=system, emitter_asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal', name=name)

    def call(action, **kwargs):
        """只把发射器参数传给发射器动作；系统级编译与保存单独调用。"""
        value = p.niagara(action, asset_path=system, emitter=name, **kwargs)
        if value.get('text', '').startswith(('Failed', 'Error')):
            raise RuntimeError(value['text'])
        return value

    call('set_emitter_property', property='SimTarget', value='GPUComputeSim')
    call('set_emitter_property', property='bLocalSpace', value='True')
    burst = call('add_module', usage='emitter_update', module_script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous', index=1)
    call('set_module_input_value', module_node=burst['node_guid'], input='Spawn Count', value=str(count))
    # 自定义 HLSL 对参数映射有副作用；同时放入 Spawn/Update 会被编译器复用为
    # 写 MapSpawn 的函数，更新时颜色/位置被丢弃。仅放 Update，首个模拟步即初始化形态。
    for usage in ['particle_update']:
        node = call('add_module', usage=usage, module_script=MODULE, index=1)['node_guid']
        call('set_module_input_value', module_node=node, input='Mode', value=str(mode))
        call('set_module_input_value', module_node=node, input='Life', value=str(life))
        call('set_module_input_binding', module_node=node, input='Index', binding='Particles.UniqueID')
        if usage == 'particle_update':
            call('set_module_input_binding', module_node=node, input='Age', binding='Particles.Age')
    # 模板已处于直接寿命模式；避免复合接口将显示名称错误映射成不存在的枚举。
    call('set_module_input_value', module_node='InitializeParticle', input='Lifetime', value=str(life))
    call('set_static_switch_value', module_node='EmitterState', input='Life Cycle Mode', value='Self')
    call('set_emitter_loop_profile', loop_behavior='Once', loop_duration=.15)
    if mesh:
        call('remove_renderer', renderer_index=0)
        call('add_renderer', **{'class': 'Mesh'})
        call('set_renderer_mesh', renderer_index=0, mesh=mesh if mesh.startswith('/') else BASE+'/Meshes/'+mesh)
    call('set_renderer_material', renderer_index=0, material=BASE+'/Materials/'+material)
    call('set_fixed_bounds', min=[-400, -240, -240], max=[400, 240, 300], enabled=True)
    diagnostic = p.niagara('get_system_diagnostics', asset_path=system, compile_first=True)
    evidence = ROOT / 'Saved/Validation/IceArrow/Production_20261016/HistoryTrailSimCache.json'
    # 只豁免已有实际粒子缓存证明的同一条路径启发式警告；其余警告仍阻断保存。
    validated_ribbon = False
    if evidence.exists():
        proof = json.loads(evidence.read_text(encoding='utf-8'))
        validated_ribbon = proof['粒子数量'] > 0 and proof['宽度厘米范围'][1] > 0 and proof['透明度范围'][1] > 0
    unresolved = [w for w in diagnostic.get('warnings', []) if not (
        validated_ribbon and w.get('emitter') == 'HistoryTrail' and w.get('source') == 'RendererAttributeInit')]
    if diagnostic.get('error_count', 1) or unresolved:
        raise RuntimeError(json.dumps(diagnostic, ensure_ascii=False))
    p.niagara('get_ordered_modules', asset_path=system, emitter=name)
    p.niagara('list_renderers', asset_path=system, emitter=name)
    p.niagara('save_system', asset_path=MODULE, only_if_dirty=False)
    p.niagara('save_system', asset_path=system, only_if_dirty=False)
    return {'layer': name, 'system': system, 'diagnostic_errors': 0, 'visual_acceptance': '待真实截图审核'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--layer', choices=list(LAYERS), required=True)
    args = parser.parse_args()
    print(json.dumps(add_mesh_layer(protocol(), SYSTEM, args.layer, *LAYERS[args.layer]), ensure_ascii=False))
