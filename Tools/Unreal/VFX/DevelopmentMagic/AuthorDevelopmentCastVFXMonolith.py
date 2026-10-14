#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""开发魔法起手纯表现作者：Monolith创建三套手部聚能、环绕与柔和释放光晕。
资产归项目公共表现包；复用已有两种能量材质，不复制平台播放器，不写伤害/碰撞。
inspect只列计划；apply只创建独占Cast目录，既有Flight/Impact完全不修改。
失败留下未启用的新资产以供审查，不删除用户资源；生成后仍须独立重载和客户端人工验收。
"""
import argparse
import importlib.util
import json
from pathlib import Path
from uuid import uuid4

ROOT=Path(__file__).resolve().parents[4]
DEST='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting'
COLORS={'Fireball':(1.8,0.35,0.035),'Frostbolt':(0.16,0.9,1.65),'ArcaneMissiles':(0.72,0.28,1.8)}


def author():
    """按真实身份检查→三份模块/系统→三份Definition顺序执行，任一编译警告/错误拒绝发布。"""
    spec=importlib.util.spec_from_file_location('casting_transport',ROOT/'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    t=importlib.util.module_from_spec(spec); spec.loader.exec_module(t); p=t.transport()
    p.editor_python("import unreal\nfor name in ['Fireball','Frostbolt','ArcaneMissiles']:\n if unreal.EditorAssetLibrary.does_asset_exist('"+DEST+"/Niagara/NS_DBA_Dev'+name+'Cast'): raise RuntimeError('本轮Cast资产已存在，不擅自覆盖')")
    results=[]
    for spell,color in COLORS.items():
        module=DEST+'/Niagara/NM_DBA_Dev'+spell+'Cast'
        hlsl=f'''#if GPU_SIMULATION
float w=pow(saturate(1.0-Age/1.0),0.7);
float q=Index*6.2831853/24.0+Age*4.0;
float r=Mode==0 ? 0.0 : Mode==1 ? 18.0+Age*12.0 : 24.0+Age*45.0;
Particles.Position=float3(cos(q)*r,sin(q)*r,sin(q*2.0)*r*0.2);
if(Mode==0) Particles.Position=float3(0,0,0);
Particles.Color=float4({color[0]},{color[1]},{color[2]},w*(Mode==0 ? 0.72 : Mode==1 ? 0.25 : 0.15));
Particles.SpriteSize=float2(Mode==0 ? 70.0 : Mode==1 ? 32.0 : 46.0,Mode==0 ? 70.0 : Mode==1 ? 32.0 : 46.0);
Particles.SpriteRotation=Index*15.0+Age*90.0;
#endif'''
        p.niagara('create_module_from_hlsl',name='Dev'+spell+'Cast',save_path=module,hlsl=hlsl,
                  inputs=[{'name':'Age','type':'float'},{'name':'Index','type':'int'},{'name':'Mode','type':'int'}],outputs=[],
                  description='项目开发魔法柔和手部聚能；无随机噪点、无碰撞和伤害')
        system=DEST+'/Niagara/NS_DBA_Dev'+spell+'Cast'
        emitters=[]
        for name,mode,count in [('Core',0,1),('Orbit',1,24),('ReleaseHalo',2,24)]:
            emitters.append({'name':name,'asset':'/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',
                'properties':{'SimTarget':'GPUComputeSim','bLocalSpace':'True'},
                'modules':[{'stage':'emitter_update','script':'/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous','inputs':{'Spawn Count':str(count)}},
                           {'stage':'particle_spawn','script':module,'bindings':{'Index':'Particles.UniqueID'},'inputs':{'Mode':str(mode),'Age':'0'}},
                           {'stage':'particle_update','script':module,'bindings':{'Age':'Particles.Age','Index':'Particles.UniqueID'},'inputs':{'Mode':str(mode)}}]})
        p.niagara('create_system_from_spec',save_path=system,spec={'emitters':emitters})
        for name in ['Core','Orbit','ReleaseHalo']:
            p.niagara('set_renderer_material',asset_path=system,emitter=name,renderer_index=0,material='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow')
            p.niagara('set_module_input_value',asset_path=system,emitter=name,module_node='InitializeParticle',input='Lifetime',value='1.0')
            p.niagara('set_static_switch_value',asset_path=system,emitter=name,module_node='EmitterState',input='Life Cycle Mode',value='Self')
            p.niagara('set_emitter_loop_profile',asset_path=system,emitter=name,loop_behavior='Once',loop_duration=0.15)
            p.niagara('set_fixed_bounds',asset_path=system,emitter=name,min=[-150]*3,max=[150]*3,enabled=True)
        p.niagara('set_fixed_bounds',asset_path=system,min=[-150]*3,max=[150]*3,enabled=True)
        report=p.niagara('get_system_diagnostics',asset_path=system,compile_first=True)
        if report.get('error_count',1) or report.get('warning_count',1): raise RuntimeError(json.dumps(report))
        p.niagara('save_system',asset_path=system,only_if_dirty=False)
        results.append({'Asset':system,'Diagnostics':report})
    p.editor_python('''import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting'
for spell in ['Fireball','Frostbolt','ArcaneMissiles']:
    cls=unreal.GamePlatformVFXAttachedDefinition
    name='DA_DBA_VFX_Dev'+spell+'Cast'
    if unreal.EditorAssetLibrary.does_asset_exist(root+'/Definitions/'+name): raise RuntimeError('定义已存在，不覆盖')
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
    d=tools.create_asset(name,root+'/Definitions',cls,factory)
    logical=unreal.GamePlatformId(); logical.set_editor_property('namespace','presentation.dba.development.magic.'+spell.lower()); logical.set_editor_property('name','cast'); logical.set_editor_property('logical_version',1)
    d.set_editor_property('logical_id',logical)
    d.set_editor_property('niagara_system',unreal.load_asset(root+'/Niagara/NS_DBA_Dev'+spell+'Cast'))
    d.set_editor_property('auto_destroy',True); d.set_editor_property('allow_pooling',True); d.set_editor_property('max_lifetime_seconds',1.1)
    if not unreal.EditorAssetLibrary.save_loaded_asset(d,False): raise RuntimeError('起手定义保存失败')
    unreal.EditorAssetLibrary.save_asset(root+'/Niagara/NM_DBA_Dev'+spell+'Cast',False)
    print('CAST_DEFINITION_SAVED',d.get_path_name())''')
    (p.EVIDENCE/'CastSystems.json').write_text(json.dumps({'Systems':results,'ReloadVerified':False,'ManualVisualAccepted':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'CompiledSystems':len(results),'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__); parser.add_argument('--apply',action='store_true'); args=parser.parse_args()
    if args.apply: author()
    else: print(json.dumps({'Systems':[DEST+'/Niagara/NS_DBA_Dev'+name+'Cast' for name in COLORS],'Modified':False},ensure_ascii=False))
