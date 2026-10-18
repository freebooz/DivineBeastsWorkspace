# -*- coding: utf-8 -*-
"""定向飞弹的Monolith原生制作配方，唯一System中有弹头与三条世界空间历史Ribbon。"""
from pathlib import Path
import argparse,importlib.util,json
ROOT=Path(__file__).resolve().parents[4];BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile';SYSTEM=BASE+'/Niagara/NS_DBA_DirectedMissile_V1'
HEAD='Particles.Position=float3(0,0,0);Particles.Scale=float3(Scale,Scale,Scale);Particles.Color=float4(Intensity,Intensity,Intensity,1);'
SPAWN='''float3 d=length(Direction)>.001 ? normalize(Direction) : float3(1,0,0);
float3 axis=abs(d.z)<.95 ? float3(0,0,1) : float3(1,0,0);float3 side=normalize(cross(axis,d));
// CPU VectorVM使用显式三次平滑，保证出生/结束横摆连续并汇入弹头。
float q=Clock*16+Phase;float begin=saturate(Clock/.10);float end=saturate((Clock-.56)/.20);
float attach=begin*begin*(3-2*begin)*(1-end*end*(3-2*end));
Particles.Position=Owner+(side*sin(q)*50+float3(0,0,cos(q)*22))*attach;
Particles.RibbonWidth=Width;Particles.RibbonLinkOrder=Particles.UniqueID;
// B通道保留出生柔化遮罩；材质仅G通道区分三尾暖色，不直接乘RGB。
Particles.Color=float4(1,Phase<.5 ? .65 : (Phase<1.5 ? .32 : .48),begin*begin*(3-2*begin),1);'''
FADE='''float t=saturate(Age/max(Life,.01));Particles.RibbonWidth=Width*(1-.65*t)*(.15+.85*Particles.Color.b);Particles.Color.a=pow(1-t,.65)*Particles.Color.b;'''
def main():
    a=argparse.ArgumentParser();a.add_argument('--phase',required=True,choices=('import','materials','system','definition','all'));a.add_argument('--reimport-own-source',action='store_true');args=a.parse_args()
    s=importlib.util.spec_from_file_location('missile_transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py');p=importlib.util.module_from_spec(s);s.loader.exec_module(p);p.EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1/Protocol'
    p.editor_python("import unreal\nassert '-DirectedMissileV1OwnedReview' in unreal.SystemLibrary.get_command_line()\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')")
    for phase in (('import','materials','system','definition') if args.phase=='all' else (args.phase,)):
        if phase!='system':
            file=str(Path(__file__).with_name('AuthorDirectedMissileV1UE.py')).replace('\\','/');result=p.editor_python('import runpy\nrunpy.run_path('+repr(file)+',init_globals='+repr(dict(PHASE=phase,REIMPORT_OWN_SOURCE=args.reimport_own_source))+')');print(phase,json.dumps(result,ensure_ascii=False)[-600:],flush=True);continue
        modules={}
        for name,hlsl,inputs in [('Head',HEAD,[('Scale','float'),('Intensity','float')]),('TrailSpawn',SPAWN,[('Owner','position'),('Direction','vec3'),('Clock','float'),('Phase','float'),('Width','float')]),('TrailFade',FADE,[('Age','float'),('Life','float'),('Width','float')])]:
            path=BASE+'/Niagara/NM_DBA_MissileV1_'+name;modules[name]=path
            existing='True' in json.dumps(p.editor_python("import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('"+path+"'))"))
            if existing:p.niagara('set_custom_hlsl_text',script_path=path,hlsl=hlsl)
            else:p.niagara('create_module_from_hlsl',name='MissileV1'+name,save_path=path,hlsl=hlsl,inputs=[dict(name=n,type=t) for n,t in inputs],outputs=[],description='定向飞弹纯视觉 '+name)
            p.editor_python("import unreal\na=unreal.load_asset('"+path+"')\nfor n in unreal.ObjectIterator(unreal.NiagaraNodeInput):\n if n.get_path_name().startswith('"+path+".'):\n  o=n.get_editor_property('exposure_options');o.set_editor_property('exposed',True);o.set_editor_property('required',True);o.set_editor_property('hidden',False);n.set_editor_property('exposure_options',o)\nunreal.EditorAssetLibrary.save_loaded_asset(a,False)")
        emitters=[dict(name='MissileHead',asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'True'},modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'1'}),dict(stage='particle_update',script=modules['Head'],bindings={'Scale':'User.HeadScale','Intensity':'User.Intensity'})],renderers=[{'class':'Mesh'}])]
        tails=[('GoldTrail',0),('AmberTrail',1.047198),('SoftGoldTrail',2.094395)]
        for name,phase in tails:
            emitters.append(dict(name=name,asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'False'},modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnRate.SpawnRate',inputs={'SpawnRate':'96'}),dict(stage='particle_spawn',script=modules['TrailSpawn'],inputs={'Phase':str(phase)},bindings={'Owner':'Engine.Owner.Position','Direction':'User.Direction','Clock':'Emitter.Age','Width':'User.TrailWidth'}),dict(stage='particle_update',script=modules['TrailFade'],bindings={'Age':'Particles.Age','Life':'Particles.Lifetime','Width':'User.TrailWidth'})],renderers=[{'class':'Ribbon'}]))
        spec=dict(user_parameters=[dict(name='User.'+n,type='float',default=str(v)) for n,v in [('HeadLifetime',.85),('TrailLifetime',1.),('HeadScale',1.),('TrailWidth',26.),('Intensity',1.)]]+[dict(name='User.Direction',type='vec3',default='1,0,0')],emitters=emitters)
        exists='True' in json.dumps(p.editor_python("import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('"+SYSTEM+"'))"))
        if not exists:p.niagara('create_system_from_spec',save_path=SYSTEM,spec=spec)
        names={e['name'] for e in p.niagara('list_emitters',asset_path=SYSTEM)['emitters']}
        if 'SoftGoldTrail' not in names:
            source=p.editor_python("import unreal\npaths=[e.get_path_name() for e in unreal.ObjectIterator(unreal.NiagaraEmitter) if e.get_outer()==unreal.load_asset('"+SYSTEM+"') and e.get_name().startswith('GoldTrail_')]\nassert len(paths)==1,paths\nprint('MISSILE_SOURCE_EMITTER_PATH='+paths[0])")
            source_path=next(o['output'].strip().split('=',1)[1] for o in source['output'] if o.get('output','').startswith('MISSILE_SOURCE_EMITTER_PATH='))
            p.niagara('add_emitter',asset_path=SYSTEM,emitter_asset=source_path,name='SoftGoldTrail')
        for name,phase in tails:p.niagara('set_module_input_value',asset_path=SYSTEM,emitter=name,module_node='NM_DBA_MissileV1_TrailSpawn',input='Phase',value=str(phase))
        for value in spec['user_parameters']:p.niagara('set_parameter_default',asset_path=SYSTEM,parameter=value['name'],value=value['default'])
        for name in ('MissileHead','GoldTrail','AmberTrail','SoftGoldTrail'):
            for r in reversed(p.niagara('list_renderers',asset_path=SYSTEM,emitter=name)['renderers']):
                if r['type']=='sprite':p.niagara('remove_renderer',asset_path=SYSTEM,emitter=name,renderer_index=r['index'])
            p.niagara('set_module_input_binding',asset_path=SYSTEM,emitter=name,module_node='InitializeParticle',input='Lifetime',binding='User.HeadLifetime' if name=='MissileHead' else 'User.TrailLifetime')
            p.niagara('set_static_switch_value',asset_path=SYSTEM,emitter=name,module_node='EmitterState',input='Life Cycle Mode',value='Self');p.niagara('set_emitter_loop_profile',asset_path=SYSTEM,emitter=name,loop_behavior='Once',loop_duration=.03 if name=='MissileHead' else .76)
            p.niagara('set_fixed_bounds',asset_path=SYSTEM,emitter=name,min=[-2500,-2500,-1200],max=[2500,2500,1200],enabled=True)
            if name=='MissileHead':p.niagara('set_renderer_mesh',asset_path=SYSTEM,emitter=name,renderer_index=0,mesh=BASE+'/Meshes/SM_DBA_DirectedMissile_V1')
            else:
                p.niagara('set_renderer_material',asset_path=SYSTEM,emitter=name,renderer_index=0,material=BASE+'/Materials/M_DBA_MissileV1_Trail');p.niagara('configure_ribbon',asset_path=SYSTEM,emitter=name,renderer_index=0,preset='trail',facing_mode='Screen',tessellation_mode='Custom',tessellation_factor=4,uv_mode='TiledFromStartOverRibbonLength',tiling_length=180,width_binding='Particles.RibbonWidth',link_order_binding='Particles.RibbonLinkOrder')
        effect=BASE+'/Scalability/NET_DBA_Review_CombatProjectile'
        if 'True' not in json.dumps(p.editor_python("import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('"+effect+"'))")):p.niagara('create_effect_type',save_path=effect,cull_reaction='Deactivate',update_frequency='High')
        p.niagara('set_effect_type',asset_path=SYSTEM,effect_type=effect);p.niagara('set_fixed_bounds',asset_path=SYSTEM,min=[-2500,-2500,-1200],max=[2500,2500,1200],enabled=True)
        result=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True);p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
        out=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1';(out/'Compile.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');(out/'NiagaraSpec.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2),encoding='utf8');assert result['error_count']==0,result
        print('MISSILE_SYSTEM_COMPILED',result['error_count'],result['warning_count'],flush=True)
if __name__=='__main__':main()
