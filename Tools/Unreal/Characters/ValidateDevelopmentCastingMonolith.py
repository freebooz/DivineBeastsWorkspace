#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""施法资源的真实FK动作门禁；Monolith读取保存的骨架/关键帧，不把截图生成当动作通过。
Manny项目网格朝向为本地+Y、单位厘米；施法关键帧右手应抬到胸前(>=125cm)并向前(>=20cm)。
同时读取Raw与Compressed姿势，避免只证明编辑器源关键帧而遗漏实际压缩数据。
这是开发动作可读性基线，不是骨骼伤害碰撞、网络复制、Cook或最终动作美术签收。
"""
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]


def validate():
    """实际采样三动作；无资产/无关键帧/错误骨架/下腹姿势均明确失败，不自动修正或跳过。"""
    spec=importlib.util.spec_from_file_location('casting_transport',ROOT/'Tools/Unreal/Characters/AuthorDevelopmentCastingMonolith.py')
    t=importlib.util.module_from_spec(spec); spec.loader.exec_module(t); p=t.transport(); reports=[]
    for spell,time in [('Fireball',0.2),('Frostbolt',0.2),('ArcaneMissiles',0.4)]:
        asset=t.ANIMS+'/AS_DBA_Dev'+spell+'Cast'
        info=p.call('animation_query',{'action':'get_sequence_info','params':{'asset_path':asset}})
        pose=p.call('animation_query',{'action':'get_animated_bone_transform','params':{'asset_path':asset,'bone_name':'hand_r','time':time,'space':'component'}})
        loc=pose['transform']['location']
        idle=p.call('animation_query',{'action':'get_animated_bone_transform','params':{'asset_path':'/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle','bone_name':'hand_r','time':0,'space':'component'}})['transform']['location']
        endpoints=[]
        for endpoint in (0,info['duration']):
            hand=p.call('animation_query',{'action':'get_animated_bone_transform','params':{'asset_path':asset,'bone_name':'hand_r','time':endpoint,'space':'component'}})['transform']['location']
            endpoints.append(sum((hand[key]-idle[key])**2 for key in ('x','y','z'))**0.5)
        # Monolith公开FK查询读取Raw；显式采样压缩数据，结果必须实际返回，不能回退到Raw冒充。
        response=p.editor_python(f'''import json, unreal
s=unreal.load_asset({asset!r})
m=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple')
o=unreal.AnimPoseEvaluationOptions()
o.set_editor_property('evaluation_type',unreal.AnimDataEvalType.COMPRESSED)
o.set_editor_property('optional_skeletal_mesh',m)
q=unreal.AnimPoseExtensions.get_anim_pose_at_time(s,{time},o)
v=unreal.AnimPoseExtensions.get_bone_pose(q,'hand_r',unreal.AnimPoseSpaces.WORLD).translation
print('COMPRESSED_POSE='+json.dumps({{'x':v.x,'y':v.y,'z':v.z}}))''')
        outputs=[item['output'].strip() for item in response.get('output',[]) if item.get('output','').startswith('COMPRESSED_POSE=')]
        if len(outputs)!=1: raise RuntimeError('未获得唯一真实压缩姿势结果')
        compressed=json.loads(outputs[0].split('=',1)[1])
        passed=not info['has_root_motion'] and loc['z']>=125 and loc['y']>=20 and compressed['z']>=125 and compressed['y']>=20 and all(abs(loc[key]-compressed[key])<1.0 for key in ('x','y','z')) and max(endpoints)<2.0
        reports.append({'Asset':asset,'TimeSeconds':time,'RawHand':loc,'CompressedHand':compressed,'IdleEndpointDistanceCm':endpoints,'Passed':passed})
    result={'Poses':reports,'Passed':all(r['Passed'] for r in reports),'ManualVisualAccepted':False,'ClientRenderAccepted':False}
    (p.EVIDENCE/'CastingPoseContract.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'Poses':reports,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))
    if not result['Passed']: raise RuntimeError('真实施法FK动作未达到胸前/前推基线，不可仅凭资产存在交付')


if __name__=='__main__': validate()
