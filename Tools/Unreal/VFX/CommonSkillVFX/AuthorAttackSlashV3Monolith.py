# -*- coding: utf-8 -*-
"""沿用唯一Monolith作者协议，只创建凝锋V3这一攻击修订的原生资源。"""
from pathlib import Path
import argparse,importlib.util,json
ROOT=Path(__file__).resolve().parents[4]
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V3/AttackSlash'
MODULE=BASE+'/Niagara/NM_DBA_SlashV3Sweep';SYSTEM=BASE+'/Niagara/NS_DBA_AttackSlash_V3'
HLSL='''float t=saturate(Age/max(.25,Life));
float r=saturate(t/.65);float sweep=r*r*(3-2*r);
float angle=.12*(sweep-.7812745);
Particles.Position=float3(0,0,0);
Particles.Scale=float3(Radius/230.,Radius/230.,Radius/230.);
Particles.MeshOrientation=float4(0,0,sin(angle*.5),cos(angle*.5));
Particles.Color=float4(Intensity,Intensity,Intensity,1);
'''
def main():
    a=argparse.ArgumentParser();a.add_argument('--phase',required=True,choices=('import','materials','system','definition','all'))
    a.add_argument('--reimport-own-source',action='store_true',help='重新导入本V3命名空间已有的自有网格和纹理，不覆盖其他资源')
    args=a.parse_args()
    s=importlib.util.spec_from_file_location('slash_v3_protocol',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py');p=importlib.util.module_from_spec(s);s.loader.exec_module(p);p.EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3/Protocol'
    p.editor_python("import unreal\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nassert unreal.SystemLibrary.get_engine_version().startswith('5.8.')")
    for phase in (('import','materials','system','definition') if args.phase=='all' else (args.phase,)):
        if phase!='system':
            file=str(Path(__file__).with_name('AuthorAttackSlashV3UE.py')).replace('\\','/')
            result=p.editor_python('import runpy\nrunpy.run_path('+repr(file)+',init_globals='+repr(dict(PHASE=phase,REIMPORT_OWN_SOURCE=args.reimport_own_source))+')');print(phase,json.dumps(result,ensure_ascii=False)[-1200:],flush=True);continue
        existing='EXISTS True' in json.dumps(p.editor_python("import unreal\nprint('EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+SYSTEM+"'))"))
        if existing:p.niagara('set_custom_hlsl_text',script_path=MODULE,hlsl=HLSL)
        else:
            p.niagara('create_module_from_hlsl',name='SlashV3Sweep',save_path=MODULE,hlsl=HLSL,inputs=[dict(name=n,type='float') for n in ('Age','Life','Radius','Intensity')],outputs=[],description='凝锋参考空间刀翼的有限扫掠，纯视觉')
            p.editor_python("import unreal\na=unreal.load_asset('"+MODULE+"')\nnodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith('"+MODULE+".')]\nassert len(nodes)==1\nn=nodes[0]\no=n.get_editor_property('exposure_options')\no.set_editor_property('exposed',True)\no.set_editor_property('required',True)\no.set_editor_property('hidden',False)\nn.set_editor_property('exposure_options',o)\nunreal.EditorAssetLibrary.save_loaded_asset(a,False)")
            emitter=dict(name='RisingCrescent',asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'True'},modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'1'}),dict(stage='particle_update',script=MODULE,bindings={'Age':'Particles.Age','Life':'User.Duration','Radius':'User.Radius','Intensity':'User.Intensity'})],renderers=[{'class':'Mesh'}])
            p.niagara('create_system_from_spec',save_path=SYSTEM,spec=dict(user_parameters=[dict(name='User.'+n,type='float',default=str(v)) for n,v in [('Duration',.85),('Radius',230.),('Intensity',1.)]],emitters=[emitter]))
        for r in reversed(p.niagara('list_renderers',asset_path=SYSTEM,emitter='RisingCrescent')['renderers']):
            if r['type']=='sprite':p.niagara('remove_renderer',asset_path=SYSTEM,emitter='RisingCrescent',renderer_index=r['index'])
        p.niagara('set_renderer_mesh',asset_path=SYSTEM,emitter='RisingCrescent',renderer_index=0,mesh=BASE+'/Meshes/SM_DBA_AttackSlash_V3')
        p.niagara('set_module_input_binding',asset_path=SYSTEM,emitter='RisingCrescent',module_node='InitializeParticle',input='Lifetime',binding='User.Duration')
        p.niagara('set_static_switch_value',asset_path=SYSTEM,emitter='RisingCrescent',module_node='EmitterState',input='Life Cycle Mode',value='Self')
        p.niagara('set_emitter_loop_profile',asset_path=SYSTEM,emitter='RisingCrescent',loop_behavior='Once',loop_duration=.12)
        for params in ({'emitter':'RisingCrescent'},{}):p.niagara('set_fixed_bounds',asset_path=SYSTEM,min=[-1100,-400,0],max=[300,700,850],enabled=True,**params)
        # 延用同一CombatCast候选语义，避免每个美术修订复制一套预算配置。
        p.niagara('set_effect_type',asset_path=SYSTEM,effect_type='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash/Scalability/NET_DBA_Review_CombatCast')
        report=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True);assert report['error_count']==0,report
        p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
        (ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3/Compile.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('V3_SYSTEM_COMPILED_SAVED',report['error_count'],report['warning_count'],flush=True)
if __name__=='__main__':main()
