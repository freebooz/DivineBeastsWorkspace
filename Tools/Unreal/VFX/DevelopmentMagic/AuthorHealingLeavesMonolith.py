#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""复用现有柳叶原生材质，制作玩家治疗用环绕上升系统及唯一附着定义。

运行前须显式确认本轮Monolith编辑器PID；只创建HealingLeaves独占资源，存在时拒绝重建。
2.4秒/56叶为客户端纯视觉，实际治疗与10秒冷却归服务器GAS，不在粒子或UI中计算。
"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
DEST='/DBAPresentationPack_Core/VFX/HealingLeaves'
MODULE=DEST+'/Niagara/NM_DBA_HealingLeavesRise'
SYSTEM=DEST+'/Niagara/NS_DBA_HealingLeaves'
HLSL='''#if GPU_SIMULATION
// 脚底局部厘米，固定粒子身份产生不同半径与相位；上升由原生粒子年龄推进。
float age=max(0.0,Age);int leaf=Index % 56;float u=saturate(age/2.4);
float angle=leaf*2.399963+age*3.4;float radius=44+(leaf % 7)*3;
float z=8+u*185+(leaf % 5)*5;
Particles.Position=float3(cos(angle)*radius,sin(angle)*radius,z);
Particles.Velocity=float3(-sin(angle)*radius*3.4,cos(angle)*radius*3.4,185.0/2.4);
Particles.SpriteSize=float2(7+(leaf % 5),13+(leaf % 7));
Particles.SpriteRotation=leaf*137.5+age*65;
float alpha=saturate(age/.15)*saturate((2.4-age)/.45);
Particles.Color=float4(.72,1.0,.65,alpha);
#endif'''


def main():
    """显式写入，通过Monolith原生编译/保存；不自动启动服务、登录或修改已有环境树叶。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');parser.add_argument('--editor-pid',type=int,required=True)
    args=parser.parse_args()
    if not args.apply:print('HEALING_LEAVES_READ_ONLY_PLAN');return
    latest=json.loads((ROOT/'Saved/Validation/IceArrowVisualFollowup/Latest.json').read_text(encoding='utf-8-sig'))
    assert latest['PID']==args.editor_pid
    spec=importlib.util.spec_from_file_location('healing_protocol',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=Path(latest['Directory'])/'HealingLeavesAuthoring'
    p.editor_python(f"import unreal,os\nassert os.getpid()=={args.editor_pid}\nassert 'DivineBeastsWorkspace/Game/DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path().replace('\\\\','/')\nassert not unreal.EditorAssetLibrary.does_asset_exist('{SYSTEM}'),'治疗系统已存在，拒绝盲目重建'")
    p.niagara('create_module_from_hlsl',name='HealingLeavesRise',save_path=MODULE,hlsl=HLSL,
        inputs=[dict(name='Age',type='float'),dict(name='Index',type='int')],outputs=[],description='服务器确认治疗后的叶片环绕上升，纯外观，局部厘米')
    p.editor_python("import unreal\ns=unreal.load_asset('"+MODULE+"')\nfor n in unreal.ObjectIterator(unreal.NiagaraNodeInput):\n if unreal.SystemLibrary.get_path_name(n).startswith(s.get_path_name()+':'):\n  o=n.get_editor_property('exposure_options');o.set_editor_property('exposed',True);o.set_editor_property('required',True);o.set_editor_property('hidden',False);n.set_editor_property('exposure_options',o)\nassert unreal.EditorAssetLibrary.save_loaded_asset(s,False)")
    emitter=dict(name='HealingLeaves',asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',
        properties=dict(SimTarget='GPUComputeSim',bLocalSpace='True'),modules=[
        dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'56'}),
        dict(stage='particle_update',script=MODULE,bindings=dict(Age='Particles.Age',Index='Particles.UniqueID'))])
    p.niagara('create_system_from_spec',save_path=SYSTEM,spec=dict(emitters=[emitter]))
    p.niagara('set_renderer_material',asset_path=SYSTEM,emitter='HealingLeaves',renderer_index=0,
        material='/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Materials/MI_DBA_Ambient_WillowLeaf')
    p.niagara('set_module_input_value',asset_path=SYSTEM,emitter='HealingLeaves',module_node='InitializeParticle',input='Lifetime',value='2.4')
    p.niagara('set_static_switch_value',asset_path=SYSTEM,emitter='HealingLeaves',module_node='EmitterState',input='Life Cycle Mode',value='Self')
    p.niagara('set_emitter_loop_profile',asset_path=SYSTEM,emitter='HealingLeaves',loop_behavior='Once',loop_duration=.1)
    for params in [dict(emitter='HealingLeaves',enabled=True),{}]:
        p.niagara('set_fixed_bounds',asset_path=SYSTEM,min=[-120,-120,-30],max=[120,120,280],**params)
    result=p.niagara('get_system_diagnostics',asset_path=SYSTEM,compile_first=True)
    assert result['error_count']==0 and result['warning_count']==0,result
    p.niagara('save_system',asset_path=SYSTEM,only_if_dirty=False)
    p.editor_python("import unreal\npath='"+DEST+"/Definitions/DA_DBA_VFX_HealingLeaves'\na=unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset('DA_DBA_VFX_HealingLeaves','"+DEST+"/Definitions',unreal.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DevFrostboltCast'))\nassert a is not None\ni=a.get_editor_property('logical_id');assert i.import_text('(Namespace=\"presentation.dba.development.magic.healing\",Name=\"impact\",LogicalVersion=1)');a.set_editor_property('logical_id',i)\na.set_editor_property('niagara_system',unreal.load_asset('"+SYSTEM+"'));a.set_editor_property('default_attach_point',unreal.Name('None'))\nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)\nprint('HEALING_LEAVES_DEFINITION_SAVED',a.get_editor_property('logical_id').export_text())")
    print('HEALING_LEAVES_SAVED',json.dumps(result,ensure_ascii=False))


if __name__=='__main__':main()
