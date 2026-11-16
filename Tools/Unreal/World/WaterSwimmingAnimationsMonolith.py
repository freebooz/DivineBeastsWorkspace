"""通过原生AnimationDataController为当前共用Kachujin骨架制作基础泳潜动作。"""
import json
import math
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve().parent
OUT=ROOT/'Saved/Validation/VillageWaterSystems20261031'
FOLDER='/DBAContentPack_Common/Characters/KachujinGRosales/Animations/Water'
IDLE='/DBAContentPack_Common/Characters/KachujinGRosales/Animations/AS_DBA_Kachujin_StandingIdle_TargetRig'
ABP='/DBAContentPack_Common/Characters/KachujinGRosales/Animations/ABP_DBA_Kachujin'

def add(a,b): return tuple(x+y for x,y in zip(a,b))
def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def mul(a,s): return tuple(x*s for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def unit(a): return mul(a,1/math.sqrt(max(dot(a,a),1.e-16)))
def vec(v): return (v.x,v.y,v.z)
def quat(q): return (q.x,q.y,q.z,q.w)
def qi(q): return (-q[0],-q[1],-q[2],q[3])
def qm(a,b):
    x,y,z,w=a;X,Y,Z,W=b
    return (w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)
def rotate(q,v): return qm(qm(q,(*v,0)),qi(q))[:3]
def between(a,b):
    a,b=unit(a),unit(b);d=dot(a,b)
    if d < -.9999:
        axis=unit(cross(a,(0,0,1) if abs(a[2])<.8 else (0,1,0)))
        return (*axis,0)
    return unit((*cross(a,b),1+d))


def author_sequences():
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
    idle=u.load_asset(IDLE);assert isinstance(idle,u.AnimSequence)
    options=u.AnimPoseEvaluationOptions();options.set_editor_property('evaluation_type',u.AnimDataEvalType.RAW)
    pose=u.AnimPoseExtensions.get_anim_pose_at_time(idle,0.0,options)
    names=list(map(str,u.AnimPoseExtensions.get_bone_names(pose)))
    local={b:u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.LOCAL) for b in names}
    component={b:u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD) for b in names}
    required=['Hips','Head','LeftShoulder','RightShoulder']+[side+bone for side in ['Left','Right'] for bone in ['Arm','ForeArm','Hand','UpLeg','Leg','Foot']]
    assert set(required)<=set(names),'当前骨架已改变，不能按另一骨架猜测写轨道'
    hips=vec(component['Hips'].translation)
    side=unit(sub(vec(component['LeftArm'].translation),vec(component['RightArm'].translation)))
    up=unit(sub(vec(component['Head'].translation),hips));forward=unit(cross(up,side))
    definitions=[('SwimIdle',0,2.4),('SwimForward',35,1.6),('Dive',135,1.6),('SwimAscend',5,1.6)]
    written=[]
    for kind,angle,duration in definitions:
        path=FOLDER+'/AS_DBA_Kachujin_'+kind
        assert not u.EditorAssetLibrary.does_asset_exist(path),'已有动作，不覆盖：'+path
        factory=u.AnimSequenceFactory();factory.set_editor_property('target_skeleton',idle.get_skeleton())
        seq=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],FOLDER,u.AnimSequence,factory)
        assert isinstance(seq,u.AnimSequence)
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        u.SystemLibrary.execute_console_command(world,'GP.Animation.InitializeDataModel '+seq.get_path_name())
        frames=round(duration*30)
        tilt=(*mul(side,math.sin(-math.radians(angle)/2)),math.cos(math.radians(angle)/2))
        def position(b): return add(hips,rotate(tilt,sub(vec(component[b].translation),hips)))
        def rotation(b): return qm(tilt,quat(component[b].rotation))
        rotations={b:[local[b].rotation]*(frames+1) for b in names}
        parent_rotation=qm(quat(component['Hips'].rotation),qi(quat(local['Hips'].rotation)))
        hip_rotation=qm(qi(parent_rotation),rotation('Hips'))
        rotations['Hips']=[u.Quat(*hip_rotation)]*(frames+1)
        for frame in range(frames+1):
            phase=2*math.pi*frame/frames
            for label,sign in [('Left',1),('Right',-1)]:
                def solve(upper,lower,end,parent,goal,pole):
                    start=position(upper);elbow=position(lower);hand=position(end)
                    l1=math.dist(start,elbow);l2=math.dist(elbow,hand)
                    ray=sub(goal,start);distance=min(l1+l2-.4,max(abs(l1-l2)+.4,math.sqrt(dot(ray,ray))))
                    axis=unit(ray);along=(l1*l1-l2*l2+distance*distance)/(2*distance)
                    bend=unit(sub(pole,mul(axis,dot(pole,axis))))
                    new_elbow=add(start,add(mul(axis,along),mul(bend,math.sqrt(max(0,l1*l1-along*along)))))
                    new_hand=add(start,mul(axis,distance))
                    upper_world=qm(between(sub(elbow,start),sub(new_elbow,start)),rotation(upper))
                    lower_world=qm(between(sub(hand,elbow),sub(new_hand,new_elbow)),rotation(lower))
                    a=qm(qi(rotation(parent)),upper_world);b=qm(qi(upper_world),lower_world)
                    rotations[upper][frame]=u.Quat(*a);rotations[lower][frame]=u.Quat(*b)
                # 平滑对称划水；左右肘始终向体侧弯曲，保持实际骨长和全部局部平移。
                sweep=.5+.5*math.cos(phase)
                reach=18+38*sweep if kind!='SwimIdle' else 8+8*sweep
                width=24+22*(1-sweep)
                goal=add(hips,add(mul(side,sign*width),add(mul(forward,reach),mul(up,20))))
                goal=add(hips,rotate(tilt,sub(goal,hips)))
                solve(label+'Arm',label+'ForeArm',label+'Hand',label+'Shoulder',goal,
                      rotate(tilt,add(mul(side,sign),mul(forward,-.2))))
                kick=math.sin(phase+(0 if sign>0 else math.pi))
                goal=add(position(label+'Foot'),rotate(tilt,add(mul(side,sign*(5+5*kick)),
                     add(mul(forward,10*kick),mul(up,(8 if kind=='SwimIdle' else 18)*(.5+.5*kick))))))
                solve(label+'UpLeg',label+'Leg',label+'Foot','Hips',goal,rotate(tilt,forward))
        controller=seq.controller;controller.open_bracket('SSLM原生共享泳潜动作',should_transact=True)
        try:
            controller.remove_all_bone_tracks();controller.set_frame_rate(u.FrameRate(30,1));controller.set_number_of_frames(u.FrameNumber(frames))
            for bone in names:
                assert controller.add_bone_curve(bone)
                keys=rotations[bone]
                for i in range(1,len(keys)):
                    if dot(quat(keys[i-1]),quat(keys[i]))<0: keys[i]=u.Quat(*mul(quat(keys[i]),-1))
                assert controller.set_bone_track_keys(bone,[local[bone].translation]*(frames+1),keys,[local[bone].scale3d]*(frames+1)),bone
        finally:
            controller.close_bracket()
        seq.set_editor_property('enable_root_motion',False)
        u.EditorAssetLibrary.set_metadata_tag(seq,'SSLM.WaterAuthor','VillageWaterSystems20261031')
        assert u.EditorAssetLibrary.save_loaded_asset(seq,False)
        written.append({'asset':path,'duration':seq.get_play_length(),'frames':frames,'bone_tracks':len(names),'pelvis_tilt_degrees':angle})
        print('SWIM_SEQUENCE_SAVED',path)
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'SwimmingSequences.json').write_text(json.dumps({'skeleton':idle.get_skeleton().get_path_name(),'sequences':written,'native_saved':True,'human_accepted':False},ensure_ascii=False,indent=2),encoding='utf-8')
