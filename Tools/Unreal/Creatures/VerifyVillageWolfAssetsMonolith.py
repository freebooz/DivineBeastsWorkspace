#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""通过Monolith核对新手村狼、导航和小队UI的真实已保存资产，不创建替代资产。

仅连接本轮显式拥有的编辑器，原生重载后读取；不登录、不生成权威怪物、不修改地图。
请求与结果保存在Saved，失败抛出并保留证据；运行/联机和Cook须另行验证。
"""
from pathlib import Path
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = ROOT / 'Saved/Validation/VillageWolvesAndPartyUI20261016'
spec = importlib.util.spec_from_file_location('fauna_protocol', ROOT / 'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
protocol = importlib.util.module_from_spec(spec)
spec.loader.exec_module(protocol)
protocol.EVIDENCE = EVIDENCE / 'Calls'


def main():
    """绑定本次PID并核对重载后的类、骨架、命名控件和真实导航投影，任一不符即失败。"""
    identity = EVIDENCE / ('EditorRepair.json' if (EVIDENCE / 'EditorRepair.json').exists() else 'Editor.json')
    editor_pid = int(json.loads(identity.read_text(encoding='utf-8-sig'))['PID'])
    native = protocol.editor_python(f'''
import unreal,os
assert os.getpid()=={editor_pid}
assert '-UIWolfRepairOwned' in unreal.SystemLibrary.get_command_line()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().startswith('/DBAWorldPack_Village/Maps/L_Village_Start.')
paths=['/DBAWorldPack_Village/AI/DA_DBA_VillageWolfAI','/DBAWorldPack_Village/Creatures/Wolf/SK_DBA_VillageWolf','/DBAWorldPack_Village/Creatures/Wolf/ABP_DBA_VillageWolf','/DBAUIPack_Core/UI/Social/WBP_DBA_PartyPanel']
assets=[unreal.load_asset(p) for p in paths]
assert all(assets)
ok,reason=unreal.EditorLoadingAndSavingUtils.reload_packages([a.get_outermost() for a in assets])
assert ok,str(reason)
assert unreal.load_asset(paths[0]).get_editor_property('brain_type')==unreal.GamePlatformAIBrainType.NONE
assert unreal.load_class(None,paths[2]+'.ABP_DBA_VillageWolf_C')
base='/DBAWorldPack_Village/Creatures/Wolf/'
for name in ['SK_DBA_VillageWolf_Skeleton','SK_DBA_VillageWolf_Anim_WolfArmature_Idle','SK_DBA_VillageWolf_Anim_WolfArmature_Walking','Wolf']:
    assert unreal.load_asset(base+name),name
bs=unreal.load_asset(base+'BS_DBA_VillageWolf')
assert bs.get_editor_property('skeleton')
assert len(bs.get_editor_property('sample_data'))==3
assert all(s.get_editor_property('animation') for s in bs.get_editor_property('sample_data'))
prefix=paths[3]+'.WBP_DBA_PartyPanel:WidgetTree.'
for name in ['SelfPlayerIdText','CopyPlayerIdButton','TargetPlayerInput','InviteButton','InvitationsButton','ArenaButton']:
    assert unreal.find_object(None,prefix+name),name
starts=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.PlayerStart)]
assert sorted((round(a.get_actor_location().x),round(a.get_actor_location().y),round(a.get_actor_location().z)) for a in starts)==[(0,0,120),(240,0,120)]
print('NATIVE_RELOAD_WOLF_PARTY_AND_PLAYERSTARTS_VERIFIED')
''')
    animation = protocol.call('animation_query', {'action':'get_anim_graph_output_connection','params':{'abp_path':'/DBAWorldPack_Village/Creatures/Wolf/ABP_DBA_VillageWolf'}})
    if not animation.get('output_connected'):
        raise RuntimeError('狼动画没有连接最终姿势')
    nav = protocol.call('ai_query', {'action':'project_point_to_navigation','params':{'point':{'x':2600,'y':0,'z':120},'extent':{'x':150,'y':150,'z':500}}})
    if not nav.get('success'):
        raise RuntimeError('出生安全区外没有真实导航落点')
    accessibility = protocol.call('ui_query', {'action':'audit_accessibility','params':{'asset_path':'/DBAUIPack_Core/UI/Social/WBP_DBA_PartyPanel'}})
    if accessibility.get('issue_count'):
        raise RuntimeError('小队新增控件仍有可访问性问题')
    result={'EditorPID':editor_pid,'Native':native,'Animation':animation,'Navigation':nav,'Accessibility':accessibility,'RuntimeGameplayVerified':False}
    (EVIDENCE/'AssetVerification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('WOLF_PARTY_NATIVE_ASSETS_VERIFIED')


if __name__ == '__main__':
    main()
