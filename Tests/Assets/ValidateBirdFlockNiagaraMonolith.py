#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P4–P6真实Niagara回归：不得以ISM、空系统或文件存在冒充GPU鸟群。
检查基础和三物种系统的单GPU发射器、网格渲染器、User参数、InputMap暴露/接线和编译诊断。
前置：正式DivineBeastsArena UE5.8编辑器Monolith本机端口9316可用。只读但可触发编译；
--reload-saved通过原生reload_packages独立冷读8个已保存包，拒绝丢弃脏包；编译可能标脏，另由作者保存。
"""
import json
import argparse
import hashlib
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/Unreal/VFX/BirdFlock'))
from BirdFlockMonolithProtocol import niagara, EVIDENCE,editor_python
from AuthorBirdFlockNiagaraMonolith import inspect_motion_contract,inspect_route_assets,route_envelope,validate_user_parameter_types,BASE as PLATFORM_BASE,MODULE
BASE='/DBAPresentationPack_Core/VFX/Environment/BirdFlock'

def main(reload_saved=False,report_stem='NiagaraValidation'):
    """只读验收四系统；显式reload_saved先原生重载8个已保存包，脏包则中止而不丢弃修改。"""
    checked=[]
    assert report_stem and all(c.isalnum() or c in '_-' for c in report_stem),'报告名不能包含目录或特殊字符'
    systems=[PLATFORM_BASE]+[BASE+'/Niagara/NS_DBA_BirdFlock_'+s for s in ('Seagull','Swallow','Eagle')]
    if reload_saved:
        paths=[MODULE]+systems+[BASE+'/Definitions/DA_DBA_VFX_BirdFlock_'+s for s in ('Seagull','Swallow','Eagle')]
        editor_python('''import unreal
paths='''+repr(paths)+'''
assets=[unreal.EditorAssetLibrary.load_asset(p) for p in paths]
assert all(assets),'冷读目标资产缺失'
packages=[a.get_outermost() for a in assets]
dirty={p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
assert not (set(paths)&dirty),'冷读拒绝丢弃脏包：'+str(set(paths)&dirty)
del assets
changed,error=unreal.EditorLoadingAndSavingUtils.reload_packages(packages,interaction_mode=unreal.ReloadPackagesInteractionMode.ASSUME_NEGATIVE)
packages=[]
assert changed and not str(error),str(error)
print('BIRD_NATIVE_SAVED_PACKAGES_RELOADED',len(paths))
''')
    contract=inspect_motion_contract(systems)
    route_assets=inspect_route_assets(systems)
    # 独立回读保存后的真实模块；源码修复或旧GPU报告不能证明Cook中的图已经更新。
    module='/GamePlatformVFX/Environment/BirdFlock/Niagara/NM_GP_BirdFlockMotion'
    shader=niagara('get_custom_hlsl_text',script_path=module)
    expected=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockMotion.hlsl').read_text(encoding='utf-8')
    assert shader.get('hlsl')==expected, '真实Niagara运动模块未更新为当前最小间距源码'
    for path in systems:
        species='Base' if path==PLATFORM_BASE else path.rsplit('_',1)[1]
        exported=niagara('export_system_spec',asset_path=path,include_values=True)
        spec=exported['spec']
        emitters=spec['emitters']
        assert len(emitters)==1, '必须一个发射器对应整个鸟群'
        emitter=emitters[0]
        assert emitter['sim_target']=='GPU','实际发射器不是GPU模式'
        renderers=emitter['renderers']
        assert len(renderers)==1 and 'Mesh' in renderers[0]['class'],'没有唯一真实网格渲染器'
        names={p['name'].removeprefix('User.') for p in spec['user_parameters']}
        validate_user_parameter_types(spec['user_parameters'])
        assert {'BirdCount','FlockRadius','FlightSpeed','FlockSeed','FlightMode','FlockMinimumSpacing'} <= names,'鸟数/飞行/最小中心距用户参数不完整'
        values={p['name'].removeprefix('User.'):float(p['default']) for p in spec['user_parameters']}
        assert int(values['FlightMode'])=={'Base':0,'Seagull':1,'Swallow':2,'Eagle':0}[species],'物种自然路线模式未保存'
        row=route_assets[path]
        for mode in (0,1,2):
            envelope=route_envelope(values['FlockRadius'],values['FlockHeight'],512,
                values['FlockMinimumSpacing'],mode,2000.0,row['meshMarginCm'])
            assert envelope['withinFixedBounds'],'当前物种/模式512越出真实XYZ固定包围盒'
        if species!='Base':
            assert row['definitionIntegers']['User.FlightMode']==int(values['FlightMode']),'Definition与系统Mode不一致'
            assert row['definitionIntegers']['User.FlockSeed']==int(values['FlockSeed']),'Definition与系统Seed不一致'
        paths=[m['script'] for m in emitter['modules']]
        assert any('NM_GP_BirdFlockMotion' in p for p in paths),'群飞HLSL没有接入实际图'
        assert any('SpawnBurst' in p for p in paths),'缺少有界一次生成，不能用持续发射率替代数量'
        diagnostic=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        assert diagnostic.get('error_count',1)==0, 'Niagara实际编译错误'
        assert diagnostic.get('warning_count',1)==0, 'Niagara编译警告未消除'
        assert all(s.get('compile_status')=='UpToDate' for e in diagnostic['emitter_stats'] for s in e['scripts']), 'GPU脚本状态不是最新已编译'
        assert spec['system_properties']['fixed_bounds'], 'GPU系统缺少明确包围盒'
        assert 'NET_GP_BirdFlock_Ambient' in spec['system_properties']['effect_type'], '鸟群预算类型不匹配'
        disk=(ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NS_GP_BirdFlock_Base.uasset') if species=='Base' else ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Niagara'/('NS_DBA_BirdFlock_'+species+'.uasset')
        assert disk.is_file() and disk.stat().st_size>1024, '系统仅在编辑器内存，未落盘'
        checked.append({'species':species,'system':path,'spec':spec,'diagnostics':diagnostic})
        print('BIRD_NIAGARA_STRUCTURE',species,json.dumps(diagnostic,ensure_ascii=False))
    report={'nativeReloadedSavedPackages':reload_saved,'sourceSha256':hashlib.sha256(expected.encode()).hexdigest(),
            'graphContract':contract,'routeAssets':route_assets,'systems':checked}
    report_path=EVIDENCE/(report_stem+('Reloaded' if reload_saved else '')+'.json')
    report_path.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_NIAGARA_STRUCTURE_PASS',len(checked),'GPU编译结果须进一步检查诊断字段，不等于人眼验收')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description='真实鸟群图与编译门禁；--reload-saved先独立冷读8个已保存包')
    parser.add_argument('--reload-saved',action='store_true')
    parser.add_argument('--report-stem',default='NiagaraValidation',help='本轮路线建议BirdFlockRoutesValidation，保留旧间距证据')
    args=parser.parse_args()
    main(args.reload_saved,args.report_stem)
