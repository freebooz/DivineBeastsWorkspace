#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""单个弧刃V2的Monolith配方：一个CPU粒子/Emitter、一个三材质Mesh、有限寿命。

所有系统与材质由Monolith/UE创建；无其它效果、无生肖授权、无生产技能覆盖。
"""
from pathlib import Path
import argparse
import importlib.util
import json

ROOT=Path(__file__).resolve().parents[4]
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash'
MODULE=BASE+'/Niagara/NM_DBA_AttackSlashV2Motion'
SYSTEM=BASE+'/Niagara/NS_DBA_AttackSlash_V2'
EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2'
HLSL='''float t=saturate(Age/max(.25,Life));
float ramp=saturate(t/.55);
float sweep=ramp*ramp*(3.0-2.0*ramp);
float angle=-.38+.52*sweep;
Particles.Position=float3(0,0,0);
Particles.Scale=float3(Radius/230.0,Radius/230.0,Radius/230.0);
Particles.MeshOrientation=float4(0,0,sin(angle*.5),cos(angle*.5));
Particles.Color=float4(Intensity,Intensity,Intensity,1.0);
'''


def protocol():
    s=importlib.util.spec_from_file_location('slash_v2_protocol',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(s);s.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'Protocol'
    p.editor_python("import unreal\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nassert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\nprint('SLASH_EDITOR_VERIFIED')")
    return p


def engine(p,phase):
    file=str(Path(__file__).with_name('AuthorAttackSlashV2UE.py')).replace('\\','/')
    r=p.editor_python('import runpy\nrunpy.run_path('+repr(file)+',init_globals='+repr(dict(PHASE=phase))+')')
    print(phase,json.dumps(r,ensure_ascii=False)[-1200:],flush=True)


def system(p):
    exists=p.editor_python("import unreal\nprint('EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+SYSTEM+"'))")
    existing='EXISTS True' in json.dumps(exists)
    if existing:p.niagara('set_custom_hlsl_text',script_path=MODULE,hlsl=HLSL)
    else:p.niagara('create_module_from_hlsl',name='AttackSlashV2Motion',save_path=MODULE,hlsl=HLSL,
        inputs=[dict(name=n,type='float') for n in ('Age','Life','Radius','Intensity')],outputs=[],description='弧刃V2纯表现扫掠，厘米/秒；无Gameplay与生肖依赖')
    p.editor_python("import unreal\na=unreal.load_asset('"+MODULE+"')\nnodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith('"+MODULE+".')]\nassert len(nodes)==1\nn=nodes[0]\no=n.get_editor_property('exposure_options')\no.set_editor_property('exposed',True)\no.set_editor_property('required',True)\no.set_editor_property('hidden',False)\nn.set_editor_property('exposure_options',o)\nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)")
    params=[dict(name='User.'+n,type='float',default=str(v)) for n,v in [('Duration',.9),('Radius',230),('Intensity',1)]]
    emitter=dict(name='SculptedBlade',asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'True'},
        modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'1'}),
                 dict(stage='particle_update',script=MODULE,bindings={'Age':'Particles.Age','Life':'User.Duration','Radius':'User.Radius','Intensity':'User.Intensity'})],renderers=[{'class':'Mesh'}])
    if not existing:p.niagara('create_system_from_spec',save_path=SYSTEM,spec=dict(user_parameters=params,emitters=[emitter]))
    for r in reversed(p.niagara('list_renderers',asset_path=SYSTEM,emitter='SculptedBlade')['renderers']):
        if r['type']=='sprite':p.niagara('remove_renderer',asset_path=SYSTEM,emitter='SculptedBlade',renderer_index=r['index'])
    p.niagara('set_renderer_mesh',asset_path=SYSTEM,emitter='SculptedBlade',renderer_index=0,mesh=BASE+'/Meshes/SM_DBA_AttackSlash_V2')
    p.niagara('set_module_input_binding',asset_path=SYSTEM,emitter='SculptedBlade',module_node='InitializeParticle',input='Lifetime',binding='User.Duration')
    p.niagara('set_static_switch_value',asset_path=SYSTEM,emitter='SculptedBlade',module_node='EmitterState',input='Life Cycle Mode',value='Self')
    p.niagara('set_emitter_loop_profile',asset_path=SYSTEM,emitter='SculptedBlade',loop_behavior='Once',loop_duration=.12)
    # 最大半径500/230、扫掠角和源AABB共同给出的紧致合法包络，不沿用旧全模板大盒。
    p.niagara('set_fixed_bounds',asset_path=SYSTEM,emitter='SculptedBlade',min=[-850,-850,0],max=[850,850,320],enabled=True)
    p.niagara('set_fixed_bounds',asset_path=SYSTEM,min=[-850,-850,0],max=[850,850,320],enabled=True)
    effect_type=BASE+'/Scalability/NET_DBA_Review_CombatCast'
    found=p.editor_python("import unreal\nprint('TYPE_EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+effect_type+"'))")
    if 'TYPE_EXISTS True' not in json.dumps(found):p.niagara('create_effect_type',save_path=effect_type,cull_reaction='Deactivate',update_frequency='Continuous')
    p.niagara('set_effect_type',asset_path=SYSTEM,effect_type=effect_type)
    d=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
    if d.get('error_count',1):raise RuntimeError('CPU原生编译失败：'+json.dumps(d,ensure_ascii=False))
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
    (EVIDENCE/'Compile.json').write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf8')
    print('SLASH_V2_SYSTEM_SAVED',json.dumps(d,ensure_ascii=False)[-1000:],flush=True)


def main():
    a=argparse.ArgumentParser(description=__doc__);a.add_argument('--phase',choices=('import','materials','system','definition','all'),required=True);args=a.parse_args();p=protocol()
    phases=('import','materials','system','definition') if args.phase=='all' else (args.phase,)
    for phase in phases:
        if phase=='system':system(p)
        else:engine(p,phase)


if __name__=='__main__':main()
