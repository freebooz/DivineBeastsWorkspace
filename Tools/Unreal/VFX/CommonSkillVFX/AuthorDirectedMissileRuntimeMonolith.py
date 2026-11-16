# -*- coding: utf-8 -*-
"""定向飞弹审核资源提升为独立运行资源；只通过Monolith制作、编译和保存UE资产。"""
from pathlib import Path
import importlib.util,json
ROOT=Path(__file__).resolve().parents[4]
REVIEW='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile'
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Runtime/DirectedMissile'
SYSTEM=BASE+'/Niagara/NS_DBA_DirectedMissile_Runtime'
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileSlotBinding'

def main():
    s=importlib.util.spec_from_file_location('transport',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py');p=importlib.util.module_from_spec(s);s.loader.exec_module(p);p.EVIDENCE=OUT/'Protocol'
    p.editor_python("import unreal\nassert '-DirectedMissileV1OwnedReview' in unreal.SystemLibrary.get_command_line()")
    p.editor_python('''import unreal
src='''+repr(REVIEW)+''';dst='''+repr(BASE)+'''
items=['Textures/T_DBA_MissileV1_Head_Color','Textures/T_DBA_MissileV1_Trail_Color','Materials/M_DBA_MissileV1_Core','Materials/M_DBA_MissileV1_Petals','Materials/M_DBA_MissileV1_Trail','Materials/M_DBA_MissileV1_AirWave','Meshes/SM_DBA_DirectedMissile_V1','Scalability/NET_DBA_Review_CombatProjectile']
for relative in items:
 path=dst+'/'+relative
 if not unreal.EditorAssetLibrary.does_asset_exist(path):
  assert unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.load_asset(src+'/'+relative))
for n in unreal.ObjectIterator(unreal.MaterialExpressionTextureSampleParameter2D):
 if n.get_path_name().startswith(dst+'/Materials/'):
  tex=n.get_editor_property('texture')
  if tex and tex.get_path_name().startswith(src):n.set_editor_property('texture',unreal.load_asset(tex.get_path_name().replace(src,dst)))
mesh=unreal.load_asset(dst+'/Meshes/SM_DBA_DirectedMissile_V1')
for i,s in enumerate(mesh.get_editor_property('static_materials')):
 key=next(k for k in ('Core','Petals','AirWave') if k in str(s.material_slot_name));mesh.set_material(i,unreal.load_asset(dst+'/Materials/M_DBA_MissileV1_'+key))
for relative in items:
 obj=unreal.load_asset(dst+'/'+relative)
 if isinstance(obj,unreal.Material):unreal.MaterialEditingLibrary.recompile_material(obj)
 assert unreal.EditorAssetLibrary.save_loaded_asset(obj,False)
print('RUNTIME_SHARED_RESOURCES_SAVED',len(items))''')
    spec=importlib.util.spec_from_file_location('review_recipe',Path(__file__).with_name('AuthorDirectedMissileV1Monolith.py'));r=importlib.util.module_from_spec(spec);spec.loader.exec_module(r)
    # 原审核尾部在0.56秒开始汇合。运行版只在出生渐入，持续跟随真实原目标至权威结束。
    spawn=r.SPAWN.replace('float end=saturate((Clock-.56)/.20);','float end=0;')
    head='float growth=saturate(Clock/.15);growth=growth*growth*(3-2*growth);'+r.HEAD.replace('float3(Scale,Scale,Scale)','float3(Scale,Scale,Scale)*growth')
    modules={}
    for name,hlsl,inputs in [('Head',head,[('Scale','float'),('Intensity','float'),('Clock','float')]),('TrailSpawn',spawn,[('Owner','position'),('Direction','vec3'),('Clock','float'),('Phase','float'),('Width','float')]),('TrailFade',r.FADE,[('Age','float'),('Life','float'),('Width','float')])]:
        path=BASE+'/Niagara/NM_DBA_MissileRuntime_'+name;modules[name]=path
        existing='True' in json.dumps(p.editor_python('import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('+repr(path)+'))'))
        if existing:p.niagara('set_custom_hlsl_text',script_path=path,hlsl=hlsl)
        else:p.niagara('create_module_from_hlsl',name='MissileRuntime'+name,save_path=path,hlsl=hlsl,inputs=[dict(name=n,type=t) for n,t in inputs],outputs=[],description='定向飞弹运行 '+name)
        p.editor_python("import unreal\na=unreal.load_asset('"+path+"')\nfor n in unreal.ObjectIterator(unreal.NiagaraNodeInput):\n if n.get_path_name().startswith('"+path+".'):\n  o=n.get_editor_property('exposure_options');o.set_editor_property('exposed',True);o.set_editor_property('required',True);o.set_editor_property('hidden',False);n.set_editor_property('exposure_options',o)\nunreal.EditorAssetLibrary.save_loaded_asset(a,False)")
    emitters=[dict(name='MissileHead',asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'True'},modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'1'}),dict(stage='particle_update',script=modules['Head'],bindings={'Scale':'User.HeadScale','Intensity':'User.Intensity','Clock':'Emitter.Age'})],renderers=[{'class':'Mesh'}])]
    for name,phase in [('GoldTrail',0),('AmberTrail',1.047198),('SoftGoldTrail',2.094395)]:
        emitters.append(dict(name=name,asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'CPUSim','bLocalSpace':'False'},modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnRate.SpawnRate',inputs={'SpawnRate':'96'}),dict(stage='particle_spawn',script=modules['TrailSpawn'],inputs={'Phase':str(phase)},bindings={'Owner':'Engine.Owner.Position','Direction':'Engine.Owner.SystemXAxis','Clock':'Emitter.Age','Width':'User.TrailWidth'}),dict(stage='particle_update',script=modules['TrailFade'],bindings={'Age':'Particles.Age','Life':'Particles.Lifetime','Width':'User.TrailWidth'})],renderers=[{'class':'Ribbon'}]))
    spec=dict(user_parameters=[dict(name='User.'+n,type='float',default=str(v)) for n,v in [('HeadLifetime',2.2),('TrailLifetime',1.),('HeadScale',1.),('TrailWidth',26.),('Intensity',1.)]],emitters=emitters)
    exists='True' in json.dumps(p.editor_python('import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('+repr(SYSTEM)+'))'))
    if not exists:p.niagara('create_system_from_spec',save_path=SYSTEM,spec=spec)
    for name in ('MissileHead','GoldTrail','AmberTrail','SoftGoldTrail'):
        for rend in reversed(p.niagara('list_renderers',asset_path=SYSTEM,emitter=name)['renderers']):
            if rend['type']=='sprite':p.niagara('remove_renderer',asset_path=SYSTEM,emitter=name,renderer_index=rend['index'])
        p.niagara('set_module_input_binding',asset_path=SYSTEM,emitter=name,module_node='InitializeParticle',input='Lifetime',binding='User.HeadLifetime' if name=='MissileHead' else 'User.TrailLifetime')
        p.niagara('set_static_switch_value',asset_path=SYSTEM,emitter=name,module_node='EmitterState',input='Life Cycle Mode',value='Self')
        p.niagara('set_emitter_loop_profile',asset_path=SYSTEM,emitter=name,loop_behavior='Once',loop_duration=.03 if name=='MissileHead' else 2.)
        p.niagara('set_fixed_bounds',asset_path=SYSTEM,emitter=name,min=[-2500,-2500,-1200],max=[2500,2500,1200],enabled=True)
        if name=='MissileHead':p.niagara('set_renderer_mesh',asset_path=SYSTEM,emitter=name,renderer_index=0,mesh=BASE+'/Meshes/SM_DBA_DirectedMissile_V1')
        else:
            p.niagara('set_renderer_material',asset_path=SYSTEM,emitter=name,renderer_index=0,material=BASE+'/Materials/M_DBA_MissileV1_Trail')
            p.niagara('configure_ribbon',asset_path=SYSTEM,emitter=name,renderer_index=0,preset='trail',facing_mode='Screen',tessellation_mode='Custom',tessellation_factor=4,uv_mode='TiledFromStartOverRibbonLength',tiling_length=180,width_binding='Particles.RibbonWidth',link_order_binding='Particles.RibbonLinkOrder')
    p.niagara('set_effect_type',asset_path=SYSTEM,effect_type=BASE+'/Scalability/NET_DBA_Review_CombatProjectile')
    p.niagara('set_fixed_bounds',asset_path=SYSTEM,min=[-2500,-2500,-1200],max=[2500,2500,1200],enabled=True)
    result=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True);assert result['error_count']==0,result
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
    p.editor_python('''import unreal
path='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_DirectedMissileFlight'
if not unreal.EditorAssetLibrary.does_asset_exist(path):
 unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_DevFireballFlight'))
d=unreal.load_asset(path)
d.set_editor_property('logical_id',unreal.GamePlatformId(namespace='presentation.dba.development.magic.directedmissile',name='flight',logical_version=1))
d.set_editor_property('niagara_system',unreal.load_asset('''+repr(SYSTEM)+'''))
d.set_editor_property('effect_type',unreal.load_asset('''+repr(BASE+'/Scalability/NET_DBA_Review_CombatProjectile')+'''))
d.set_editor_property('require_fixed_bounds',True)
assert unreal.EditorAssetLibrary.save_loaded_asset(d,False)
print('RUNTIME_DEFINITION_SAVED')''')
    (OUT/'RuntimeCompile.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    (OUT/'RuntimeSpec.json').write_text(json.dumps(spec,ensure_ascii=False,indent=2),encoding='utf8')
    print('MISSILE_RUNTIME_SAVED',result['error_count'],result['warning_count'])
if __name__=='__main__':main()
