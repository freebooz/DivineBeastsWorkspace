#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群单次发射生命周期门禁：防止Loop Once被System生命周期覆盖而悄悄反复Burst。

读取锁定UE5.8的真实Niagara图、EmitterState输入和编译诊断，不创建/替换系统。
Self/Once的NewEnumerator1值来自本引擎实测枚举映射；引擎升级必须重新核对，不能猜测。
此静态门禁须与10分钟真实GPU数量采样同时使用，不能替代动态验证。
"""
from pathlib import Path
import json
from BirdFlockMonolithProtocol import ROOT,niagara


def unpack(value):
    return json.loads(value['result']) if isinstance(value,dict) and isinstance(value.get('result'),str) else value


def main():
    systems=['/GamePlatformVFX/Environment/BirdFlock/Niagara/NS_GP_BirdFlock_Base']
    systems += ['/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_'+s
                for s in ('Seagull','Swallow','Eagle')]
    rows=[]
    for path in systems:
        modules=unpack(niagara('get_ordered_modules',asset_path=path,emitter='BirdFlock'))
        states=[m for m in modules if m.get('function_name')=='EmitterState']
        bursts=[m for m in modules if m.get('function_name')=='SpawnBurst_Instantaneous']
        if len(states)!=1 or len(bursts)!=1:
            raise RuntimeError('必须恰好一个EmitterState和Burst模块：'+path)
        values=unpack(niagara('get_module_inputs',asset_path=path,emitter='BirdFlock',module_node='EmitterState'))
        by_name={v['name']:v for v in values}
        lifecycle=by_name.get('Life Cycle Mode',{})
        loop=by_name.get('Loop Behavior',{})
        duration=by_name.get('Loop Duration',{})
        if lifecycle.get('override_value') not in ('Self','NewEnumerator1'):
            raise RuntimeError('发射器仍由System控制，会周期性增生：'+path+' '+repr(lifecycle))
        if loop.get('override_value') not in ('Once','NewEnumerator1'):
            raise RuntimeError('发射器不是单次循环：'+path+' '+repr(loop))
        # Monolith的is_hidden来自StackFunctionStaticSwitchPins/CompileConstantResolver，
        # 是编辑器图可见性投影，不等于实际GPU执行分支；保留观测值，持续运行计数才是动态门禁。
        if float(duration.get('override_value','0'))!=86400.0:
            raise RuntimeError('本轮单次发射器持续时长配置不符：'+path)
        diagnostics=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        if diagnostics.get('error_count',1)!=0 or diagnostics.get('warning_count',1)!=0:
            raise RuntimeError('Niagara编译诊断非零：'+path)
        rows.append({'system':path,'lifeCycleMode':'Self','loopBehavior':'Once',
            'durationSeconds':86400.0,'stateModules':1,'burstModules':1,
            'loopInputHiddenInEditor':loop.get('is_hidden'),
            'compileErrors':0,'compileWarnings':0})
        print('BIRD_SINGLE_BURST_VALIDATED',path)
    out=ROOT/'Saved/Validation/BirdFlockP4P7';out.mkdir(parents=True,exist_ok=True)
    (out/'EmitterLifecycle.json').write_text(json.dumps({'status':'passed','systems':rows},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_SINGLE_BURST_GATE_PASSED',len(rows))


if __name__=='__main__':
    main()
