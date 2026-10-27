# -*- coding: utf-8 -*-
"""只生成概念对照用的掌心朝上静止姿势；正式施法动画保持原稿。
角色骨骼名只属于审核Rig，不进入通用Niagara。双臂按原骨长解IK。
"""
import unreal as u,math,runpy
A=runpy.run_path('E:/work/2026/DivineBeastsWorkspace/Tools/Unreal/VFX/DevelopmentMagic/AuthorPalmRedesignReviewUE58.py')
BASE=A['BASE'];p=BASE+'/Animations/AS_DBA_PalmConceptPose_Review'
idle=u.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle')
o=u.AnimPoseEvaluationOptions();o.set_editor_property('evaluation_type',u.AnimDataEvalType.RAW)
pose=u.AnimPoseExtensions.get_anim_pose_at_time(idle,0,o)
names=u.AnimPoseExtensions.get_bone_names(pose)
loc={str(b):u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.LOCAL) for b in names}
ws={str(b):u.AnimPoseExtensions.get_bone_pose(pose,b,u.AnimPoseSpaces.WORLD) for b in names}
def v(x):return(x.x,x.y,x.z)
def q(x):return(x.x,x.y,x.z,x.w)
def add(a,b):return tuple(x+y for x,y in zip(a,b))
def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def mul(a,s):return tuple(x*s for x in a)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def unit(a):return mul(a,1/math.sqrt(dot(a,a)))
def cross(a,b):return(a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def qm(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return(w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)
def qi(a):return(-a[0],-a[1],-a[2],a[3])
def between(a,b):
 a,b=unit(a),unit(b);d=dot(a,b)
 if d<-.9999:return (*unit(cross(a,(0,0,1) if abs(a[2])<.8 else (0,1,0))),0)
 return unit((*cross(a,b),1+d))
def rotate(a,b):return qm(qm(a,(*b,0)),qi(a))[:3]
rot={}
for side,sign in [('l',1),('r',-1)]:
 upper,lower,hand,clav=['upperarm_'+side,'lowerarm_'+side,'hand_'+side,'clavicle_'+side]
 start=v(ws[upper].translation);elbow0=v(ws[lower].translation);hand0=v(ws[hand].translation)
 goal=(sign*32,30,111)
 l1=math.sqrt(dot(sub(elbow0,start),sub(elbow0,start)));l2=math.sqrt(dot(sub(hand0,elbow0),sub(hand0,elbow0)))
 direction=unit(sub(goal,start));d=min(math.sqrt(dot(sub(goal,start),sub(goal,start))),l1+l2-.1)
 goal=add(start,mul(direction,d));bend=(sign,-.5,-.6);bend=unit(sub(bend,mul(direction,dot(bend,direction))))
 a=(l1*l1-l2*l2+d*d)/(2*d);elbow=add(add(start,mul(direction,a)),mul(bend,math.sqrt(max(0,l1*l1-a*a))))
 uq=qm(between(sub(elbow0,start),sub(elbow,start)),q(ws[upper].rotation))
 lq=qm(between(sub(hand0,elbow0),sub(goal,elbow)),q(ws[lower].rotation))
 rot[upper]=qm(qi(q(ws[clav].rotation)),uq);rot[lower]=qm(qi(uq),lq)
 f=unit(sub(v(ws['middle_01_'+side].translation),hand0))
 across=sub(v(ws['pinky_01_'+side].translation),v(ws['index_01_'+side].translation))
 n=unit(mul(cross(f,across),sign))
 delta=between(n,(0,0,1));forward=rotate(delta,f)
 yaw=math.pi/2-math.atan2(forward[1],forward[0]);turn=(0,0,math.sin(yaw/2),math.cos(yaw/2))
 hq=qm(qm(turn,delta),q(ws[hand].rotation));rot[hand]=qm(qi(lq),hq)
factory=u.AnimSequenceFactory();factory.set_editor_property('target_skeleton',idle.get_skeleton())
seq=A['make'](p,u.AnimSequence,factory)
u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(),'GP.Animation.InitializeDataModel '+seq.get_path_name())
c=seq.controller;c.open_bracket('Palm concept review pose',should_transact=True)
try:
 c.remove_all_bone_tracks();c.set_frame_rate(u.FrameRate(30,1));c.set_number_of_frames(u.FrameNumber(1))
 for b in names:
  ref=loc[str(b)];c.add_bone_curve(b);r=u.Quat(*rot[str(b)]) if str(b) in rot else ref.rotation
  assert c.set_bone_track_keys(b,[ref.translation]*2,[r]*2,[ref.scale3d]*2)
finally:c.close_bracket()
A['save'](seq);print('PALM_REVIEW_POSE',p)
