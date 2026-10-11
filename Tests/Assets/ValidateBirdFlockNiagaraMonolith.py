#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P4–P6真实Niagara回归：不得以ISM、空系统或文件存在冒充GPU鸟群。
逐物种检查单GPU发射器、网格渲染器、正式模型、User.BirdCount和编译诊断。
前置：正式DivineBeastsArena UE5.8编辑器Monolith本机端口9316可用。只读但可触发编译。
"""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/Unreal/VFX/BirdFlock'))
from BirdFlockMonolithProtocol import niagara, EVIDENCE
BASE='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'

def main():
    checked=[]
    for species in ('Seagull','Swallow','Eagle'):
        path=BASE+'/Niagara/NS_DBA_BirdFlock_'+species
        exported=niagara('export_system_spec',asset_path=path,include_values=True)
        spec=exported['spec']
        emitters=spec['emitters']
        assert len(emitters)==1, '必须一个发射器对应整个鸟群'
        emitter=emitters[0]
        assert emitter['sim_target']=='GPU','实际发射器不是GPU模式'
        renderers=emitter['renderers']
        assert len(renderers)==1 and 'Mesh' in renderers[0]['class'],'没有唯一真实网格渲染器'
        names={p['name'].removeprefix('User.') for p in spec['user_parameters']}
        assert {'BirdCount','FlockRadius','FlightSpeed','FlockSeed','FlightMode'} <= names,'鸟数/飞行用户参数不完整'
        paths=[m['script'] for m in emitter['modules']]
        assert any('NM_GP_BirdFlockMotion' in p for p in paths),'群飞HLSL没有接入实际图'
        assert any('SpawnBurst' in p for p in paths),'缺少有界一次生成，不能用持续发射率替代数量'
        diagnostic=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        assert diagnostic.get('error_count',1)==0, 'Niagara实际编译错误'
        assert diagnostic.get('warning_count',1)==0, 'Niagara编译警告未消除'
        assert all(s.get('compile_status')=='UpToDate' for e in diagnostic['emitter_stats'] for s in e['scripts']), 'GPU脚本状态不是最新已编译'
        assert spec['system_properties']['fixed_bounds'], 'GPU系统缺少明确包围盒'
        assert 'NET_GP_BirdFlock_Ambient' in spec['system_properties']['effect_type'], '鸟群预算类型不匹配'
        disk=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Niagara'/('NS_DBA_BirdFlock_'+species+'.uasset')
        assert disk.is_file() and disk.stat().st_size>1024, '系统仅在编辑器内存，未落盘'
        checked.append({'species':species,'system':path,'spec':spec,'diagnostics':diagnostic})
        print('BIRD_NIAGARA_STRUCTURE',species,json.dumps(diagnostic,ensure_ascii=False))
    (EVIDENCE/'NiagaraValidation.json').write_text(json.dumps(checked,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_NIAGARA_STRUCTURE_PASS',len(checked),'GPU编译结果须进一步检查诊断字段，不等于人眼验收')

if __name__=='__main__':
    main()
