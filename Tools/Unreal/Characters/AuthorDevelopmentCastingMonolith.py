#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""项目开发施法动作作者；仅通过Monolith在正式UE5.8工程中制作真实动画。
inspect不改资产；apply创建本轮独占三序列/三Montage，保留既有移动动画图。
动画为原创开发动作，不含根运动、伤害Notify或服务器时序。生产美术替换保持稳定软引用。
修改已有动画图前检查脏包并备份；任何动作失败立即终止，不自动重放或删除用户资产。
"""
import argparse
import importlib.util
import json
import shutil
from pathlib import Path
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[3]
ANIMS = '/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting'
ABP = '/DBAContentPack_Common/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion'
SPELLS = {'Fireball': 0.7, 'Frostbolt': 0.7, 'ArcaneMissiles': 1.0}
SKELETON='/DBAContentPack_Common/Mannequins/UE5/Meshes/SK_Mannequin'


def transport():
    """复用已存在的失败关闭协议；每次使用独立证据目录，不能借用旧作者验证。"""
    path = ROOT / 'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py'
    spec = importlib.util.spec_from_file_location('casting_monolith_protocol', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.EVIDENCE = ROOT / 'Saved/Validation/CastingMonster20261013' / ('Authoring-' + str(uuid4()))
    module.editor_python(r'''import unreal
assert '-CastingMonsterOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
assert unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path()).replace(chr(92),'/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
print('CASTING_EDITOR_IDENTITY_VERIFIED')''')
    return module


def animation_source(refine=False):
    """保持完整Manny实际待机姿态/局部骨长；只旋转上身，零缩放/根位移/伤害通知。
    平滑聚拢、前推及回收；Arcane保持引导姿势并有低幅脉动，动画是独立可编辑资产。
    """
    source=r'''import math, unreal
allow_existing=__ALLOW_EXISTING__
root='/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting'
idle=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle')
if not isinstance(idle,unreal.AnimSequence): raise RuntimeError('缺少真实公共参考骨架')
skeleton=idle.get_skeleton()
options=unreal.AnimPoseEvaluationOptions()
options.set_editor_property('evaluation_type',unreal.AnimDataEvalType.RAW)
pose=unreal.AnimPoseExtensions.get_anim_pose_at_time(idle,0.0,options)
tools=unreal.AssetToolsHelpers.get_asset_tools()
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# UE5左右手骨局部轴镜像，不凭Euler正负号猜测抬手；按真实参考骨长解二段臂链，保持所有局部平移。
def vec(v): return (v.x,v.y,v.z)
def quat(q): return (q.x,q.y,q.z,q.w)
def add(a,b): return tuple(a[i]+b[i] for i in range(3))
def sub(a,b): return tuple(a[i]-b[i] for i in range(3))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def unit(a): return mul(a,1/math.sqrt(dot(a,a)))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def qmul(a,b):
    x,y,z,w=a; X,Y,Z,W=b
    return (w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)
def qinv(a): return (-a[0],-a[1],-a[2],a[3])
def qnorm(a): return tuple(x/math.sqrt(dot(a,a)) for x in a)
def between(a,b):
    a,b=unit(a),unit(b); d=dot(a,b)
    if d<-0.9999:
        axis=unit(cross(a,(0,0,1) if abs(a[2])<0.8 else (0,1,0)))
        return (*axis,0)
    return qnorm((*cross(a,b),1+d))
def slerp(a,b,t):
    d=dot(a,b)
    if d<0: b=tuple(-x for x in b); d=-d
    if d>0.9995: return qnorm(tuple(a[i]*(1-t)+b[i]*t for i in range(4)))
    angle=math.acos(max(-1,min(1,d))); s=math.sin(angle)
    return tuple((a[i]*math.sin((1-t)*angle)+b[i]*math.sin(t*angle))/s for i in range(4))
local={str(b):unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.LOCAL) for b in unreal.AnimPoseExtensions.get_bone_names(pose)}
component={str(b):unreal.AnimPoseExtensions.get_bone_pose(pose,b,unreal.AnimPoseSpaces.WORLD) for b in unreal.AnimPoseExtensions.get_bone_names(pose)}
def arm_keys(spell,frames):
    result={b:[] for b in ['upperarm_l','lowerarm_l','upperarm_r','lowerarm_r']}
    for frame in range(frames+1):
        u=frame/frames
        w=max(0,min(u/0.18,1)*min((1-u)/0.22,1)); w=w*w*(3-2*w)
        push=math.sin(min(u/0.6,1)*math.pi)
        for suffix,side in [('l',1),('r',-1)]:
            upper,lower,hand,clav=['upperarm_'+suffix,'lowerarm_'+suffix,'hand_'+suffix,'clavicle_'+suffix]
            start=vec(component[upper].translation); elbow_ref=vec(component[lower].translation); hand_ref=vec(component[hand].translation)
            goal=(side*15,35+18*push,142)
            if spell=='Frostbolt': goal=(side*12,49,150) if side==-1 else (45,14,115)
            if spell=='ArcaneMissiles': goal=(side*23,43+2*math.sin(u*math.pi*6),145)
            l1=math.sqrt(dot(sub(elbow_ref,start),sub(elbow_ref,start))); l2=math.sqrt(dot(sub(hand_ref,elbow_ref),sub(hand_ref,elbow_ref)))
            direction=unit(sub(goal,start)); distance=min(math.sqrt(dot(sub(goal,start),sub(goal,start))),l1+l2-0.1)
            goal=add(start,mul(direction,distance))
            bend=(side,-0.2,-0.8); bend=unit(sub(bend,mul(direction,dot(bend,direction))))
            a=(l1*l1-l2*l2+distance*distance)/(2*distance); h=math.sqrt(max(0,l1*l1-a*a))
            elbow=add(add(start,mul(direction,a)),mul(bend,h))
            uq=qmul(between(sub(elbow_ref,start),sub(elbow,start)),quat(component[upper].rotation))
            lq=qmul(between(sub(hand_ref,elbow_ref),sub(goal,elbow)),quat(component[lower].rotation))
            target_upper=qmul(qinv(quat(component[clav].rotation)),uq); target_lower=qmul(qinv(uq),lq)
            for bone,target in [(upper,target_upper),(lower,target_lower)]:
                result[bone].append(unreal.Quat(*slerp(quat(local[bone].rotation),target,w)))
    return result
for spell,duration in [('Fireball',0.7),('Frostbolt',0.7),('ArcaneMissiles',1.0)]:
    name='AS_DBA_Dev'+spell+'Cast'; path=root+'/'+name
    exists=unreal.EditorAssetLibrary.does_asset_exist(path)
    if exists and not allow_existing: raise RuntimeError('已有施法动作需独立评审后精修，不覆盖: '+path)
    if exists:
        sequence=unreal.load_asset(path)
        assert isinstance(sequence,unreal.AnimSequence) and sequence.get_skeleton()==skeleton
    else:
        factory=unreal.AnimSequenceFactory(); factory.set_editor_property('target_skeleton',skeleton)
        sequence=tools.create_asset(name,root,unreal.AnimSequence,factory)
    if not sequence: raise RuntimeError('创建动作失败')
    unreal.SystemLibrary.execute_console_command(world,'GP.Animation.InitializeDataModel '+sequence.get_path_name())
    controller=sequence.controller; frames=round(duration*30)
    controller.open_bracket('原创开发施法上身动作',should_transact=True)
    try:
        controller.remove_all_bone_tracks()
        controller.set_frame_rate(unreal.FrameRate(30,1)); controller.set_number_of_frames(unreal.FrameNumber(frames))
        solved=arm_keys(spell,frames)
        for bone in unreal.AnimPoseExtensions.get_bone_names(pose):
            ref=unreal.AnimPoseExtensions.get_bone_pose(pose,bone,unreal.AnimPoseSpaces.LOCAL)
            if not controller.add_bone_curve(bone): raise RuntimeError('骨骼轨道失败: '+str(bone))
            keys=[]
            for frame in range(frames+1):
                u=frame/frames
                # 起止严格回到已有待机，已解臂链之外保持待机局部姿势；腿/根由基础状态机保留。
                keys.append(solved[str(bone)][frame] if str(bone) in solved else ref.rotation)
            if not controller.set_bone_track_keys(bone,[ref.translation]*(frames+1),keys,[unreal.Vector(1,1,1)]*(frames+1)):
                raise RuntimeError('骨骼关键帧失败: '+str(bone))
    finally: controller.close_bracket()
    sequence.set_editor_property('enable_root_motion',False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sequence,False): raise RuntimeError('动作保存失败')
    print('CAST_SEQUENCE_SAVED',path,sequence.get_play_length())
print('CAST_SKELETON',skeleton.get_path_name())'''
    return source.replace('__ALLOW_EXISTING__','True' if refine else 'False')


def refine_owned_sequences():
    """实测FK或待机衔接未达合同后的受控整改；先保护脏包及备份本轮动作，不改用户已有序列。"""
    p=transport()
    p.editor_python("import unreal\nassert not any(p.get_name().startswith('"+ANIMS+"/') for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()),'本轮动作有未保存编辑，停止覆盖'")
    directory=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content/Mannequins/DBA/Animations/DevelopmentCasting'
    for spell in SPELLS: shutil.copy2(directory/('AS_DBA_Dev'+spell+'Cast.uasset'),p.EVIDENCE/('AS_DBA_Dev'+spell+'Cast.before.uasset'))
    p.editor_python(animation_source(True))
    print(json.dumps({'RefinedSequences':3,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


def author_animations():
    """第一次创建新资产；Montage/槽位均走Monolith动画动作，保留已有移动状态机。"""
    p = transport()
    p.editor_python(animation_source())
    skeleton_info = p.call('animation_query', {'action': 'get_sequence_info', 'params': {'asset_path': ANIMS + '/AS_DBA_DevFireballCast'}})
    print(json.dumps(skeleton_info, ensure_ascii=False))
    # 实际骨架路径由编辑器读取；不根据另一工程的Manny身份猜测。
    result = p.editor_python("import unreal\nprint('CAST_SKELETON_PATH='+unreal.load_asset('" + ANIMS + "/AS_DBA_DevFireballCast').get_skeleton().get_path_name())")
    (p.EVIDENCE / 'SkeletonReadback.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    # Montage由主执行器根据实际读取的骨架及schema创建；本入口不猜测返回文本封装结构。
    print(json.dumps({'SequencesCreated':3,'MontagesPending':True,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


def author_montages_and_graph(create_montages=True):
    """保存三Montage，备份/保护旧ABP，以上身分骨混合保留下半身移动，不重建已有状态机。"""
    p=transport()
    guard="assert not unreal.EditorAssetLibrary.does_asset_exist('"+ANIMS+"/AM_DBA_Dev'+name+'Cast'),'Montage已存在，不覆盖'" if create_montages else "montage=unreal.load_asset('"+ANIMS+"/AM_DBA_Dev'+name+'Cast'); assert isinstance(montage,unreal.AnimMontage) and montage.get_skeleton()==sk"
    p.editor_python("import unreal\nsk=unreal.load_asset('"+SKELETON+"')\nfor name in ['Fireball','Frostbolt','ArcaneMissiles']:\n seq=unreal.load_asset('"+ANIMS+"/AS_DBA_Dev'+name+'Cast')\n assert isinstance(seq,unreal.AnimSequence) and seq.get_skeleton()==sk\n "+guard+"\nprint('CAST_SKELETON_AND_SEQUENCES_VERIFIED')")
    for spell in (SPELLS if create_montages else []):
        montage=ANIMS+'/AM_DBA_Dev'+spell+'Cast'
        p.call('animation_query',{'action':'create_montage_from_sections','params':{
            'asset_path':montage,'skeleton_path':SKELETON,'slot_name':'DefaultSlot',
            'sections':[{'name':'Cast','anim_path':ANIMS+'/AS_DBA_Dev'+spell+'Cast'}],
            'blend':{'blend_in_time':0.07,'blend_out_time':0.12,'enable_auto_blend_out':True}}})
        p.call('blueprint_query',{'action':'save_asset','params':{'asset_path':montage}})
    # 不保存/覆盖用户的未提交编辑器改动；磁盘备份是额外回退，不代替脏包保护。
    p.editor_python("import unreal\nassert '"+ABP+"' not in {p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()},'已有未保存动画图，停止覆盖'\nprint('CAST_ABP_NOT_DIRTY')")
    original=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Common/DBAContentPack_Common/Content/Mannequins/DBA/Animations/ABP_DBA_WorldLocomotion.uasset'
    shutil.copy2(original,p.EVIDENCE/'ABP_DBA_WorldLocomotion.before.uasset')
    def animation(action,**params): return p.call('animation_query',{'action':action,'params':params})
    old=animation('get_anim_graph_output_connection',abp_path=ABP)
    if not old.get('output_connected') or not old.get('source_node'): raise RuntimeError('已有移动图没有有效输出，不重建掩盖错误')
    animation('add_save_cached_pose',asset_path=ABP,cache_name='DBA.Development.CastBase',source_node=old['source_node'],position_x=400,position_y=-250)
    base=animation('add_use_cached_pose',asset_path=ABP,cache_name='DBA.Development.CastBase',position_x=600,position_y=-100)['node_name']
    slotbase=animation('add_use_cached_pose',asset_path=ABP,cache_name='DBA.Development.CastBase',position_x=600,position_y=150)['node_name']
    slot=animation('add_slot_node',asset_path=ABP,slot_name='DefaultSlot',source_node=slotbase,position_x=850,position_y=150)['node_name']
    layer=animation('add_anim_graph_node',asset_path=ABP,node_type='LayeredBoneBlend',position_x=1100,position_y=0)['node_name']
    animation('set_layered_blend_bones',asset_path=ABP,node_id=layer,layers=[{'bones':[{'bone':'spine_01','depth':1}]}])
    animation('set_anim_graph_node_property',asset_path=ABP,node_id=layer,property_path='BlendWeights',value='(1.0)',graph_name='AnimGraph')
    animation('connect_anim_graph_pins',asset_path=ABP,source_node=base,source_pin='Pose',target_node=layer,target_pin='BasePose',compile=False)
    animation('connect_anim_graph_pins',asset_path=ABP,source_node=slot,source_pin='Pose',target_node=layer,target_pin='BlendPoses_0',compile=False)
    animation('set_output_pose_source',asset_path=ABP,source_node=layer)
    report=p.call('blueprint_query',{'action':'compile_blueprint','params':{'asset_path':ABP}})
    if report.get('error_count',1) or report.get('warning_count',1): raise RuntimeError('施法图编译不干净: '+json.dumps(report))
    p.call('blueprint_query',{'action':'save_asset','params':{'asset_path':ABP}})
    after=animation('get_anim_graph_output_connection',abp_path=ABP)
    if not after.get('output_connected') or after.get('source_node')!=layer: raise RuntimeError('保存后的上身施法输出未连接')
    (p.EVIDENCE/'CastingGraph.json').write_text(json.dumps({'Before':old,'After':after,'Compile':report,'ManualVisualAccepted':False,'ReloadVerified':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'MontagesCreated':3 if create_montages else 0,'UpperBodyGraphSaved':True,'Evidence':str(p.EVIDENCE)},ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply-animations', action='store_true')
    parser.add_argument('--apply-montages', action='store_true')
    parser.add_argument('--apply-graph',action='store_true',help='只接入已经保存的三Montage，不重建已成功资产')
    parser.add_argument('--refine-sequences',action='store_true',help='备份后修正本轮三动作臂链，不修改其他动画')
    args = parser.parse_args()
    if args.apply_animations: author_animations()
    elif args.apply_montages: author_montages_and_graph()
    elif args.apply_graph: author_montages_and_graph(False)
    elif args.refine_sequences: refine_owned_sequences()
    else: print(json.dumps({'Assets':[ANIMS+'/AS_DBA_Dev'+name+'Cast' for name in SPELLS], 'Modified':False},ensure_ascii=False))
