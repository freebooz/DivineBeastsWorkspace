# -*- coding:utf-8 -*-
"""SSLM火冰V3只读视觉检查：复用既有RuntimeProof审核驱动，原生1920x1080固定曝光截图。
仅在本次拥有的Editor/临时PIE生成审核对象，严格使用已保存的0.5秒掌心参数；不修改任何V3资产、正式技能或服务器。
飞行由本地审核轨迹驱动，不能冒充真实网络弹体。失败保留报告，结束只关闭本次PIE。
"""
import unreal,time,json,traceback,hashlib,math,os
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
OUT=ROOT/'Saved/Validation/FireIceV3Inspection20261026'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight/Niagara/'
STAGES=[('FirePalms','NS_DBA_FirePalmCast_V3',True),('IcePalms','NS_DBA_IcePalmCast_V3',True),('FireFlight','NS_DBA_FireOrbFlight_V3',False),('IceFlight','NS_DBA_IceCrystalFlight_V3',False)]
class CastProof:
 def __init__(self):
  owner=json.loads((ROOT/'Saved/Validation/FireballV3P10304_20261026/EditorStart.json').read_text(encoding='utf-8-sig')); assert owner['PID']==os.getpid(),'只允许本次拥有的审核编辑器'; assert Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
  assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
  assert not hasattr(unreal,'_castflight_v3inspection'),'每个独占审核进程只执行一次，避免旧World引用阻止GC'
  assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
  # 本次只读诊断的同步编译会把四份Niagara派生缓存标为dirty；不保存这些原稿。
  allowed={BASE+n for _,n,_ in STAGES}
  dirty={p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
  assert dirty.issubset(allowed), '存在非本次诊断产生的未保存内容：'+str(dirty-allowed)
  assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
  self.assets={k:unreal.load_asset(BASE+n) for k,n,_ in STAGES}
  assert all(isinstance(a,unreal.NiagaraSystem) for a in self.assets.values())
  OUT.mkdir(parents=True,exist_ok=False)
  self.report={'status':'starting','engine':unreal.SystemLibrary.get_engine_version(),'source':'native UE5.8 PIE','runtimeKey1Bound':False,'scriptedFlightTrajectory':True,'manualAccepted':False,'frames':[],'errors':[]}
  self.editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
  self.level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
  self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
  def spawn(cls,name,pos):
   a=self.editor.spawn_actor_from_class(cls,unreal.Vector(*pos));assert a,name;a.set_actor_label(name);return a
  floor=spawn(unreal.StaticMeshActor,'CastV3Floor',(0,0,-5));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(unreal.Vector(65,65,1))
  for name,yaw,pitch,power in [('Key',-40,-45,5.0),('Fill',120,-25,1.8)]:
   light=spawn(unreal.DirectionalLight,'CastV3'+name,(0,0,450));light.set_actor_rotation(unreal.Rotator(pitch=pitch,yaw=yaw,roll=0),False);light.light_component.set_editor_property('intensity',power)
  hero=spawn(unreal.SkeletalMeshActor,'CastV3Hero',(0,0,0))
  hero.set_actor_rotation(unreal.Rotator(pitch=0,yaw=145,roll=0),False)
  mesh=hero.get_component_by_class(unreal.SkeletalMeshComponent)
  mesh.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
  anim=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast')
  mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);mesh.set_animation(anim);mesh.set_position(.30,False)
  for i in range(2):
   a=spawn(unreal.NiagaraActor,'CastV3Fx'+str(i),(0,0,120));c=a.get_component_by_class(unreal.NiagaraComponent);c.set_auto_activate(False)
  camera=spawn(unreal.SceneCapture2D,'CastV3Camera',(200,-350,210))
  cap=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
  cap.set_editor_property('capture_every_frame',False);cap.set_editor_property('capture_on_movement',False)
  cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR);cap.set_editor_property('fov_angle',44.)
  pp=cap.get_editor_property('post_process_settings')
  for key,value in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':.0,'override_bloom_intensity':True,'bloom_intensity':.12,'override_motion_blur_amount':True,'motion_blur_amount':0.0}.items():pp.set_editor_property(key,value)
  cap.set_editor_property('post_process_settings',pp)
  self.world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
  self.started=time.monotonic();self.state='wait';self.stage=-1;self.frame=0;self.last=self.started
  self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist();self.level.editor_request_begin_play()
 def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
 def camera(self,pos,target):
  self.camActor.set_actor_location(pos,False,False);self.camActor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,target),False)
 def next_stage(self):
  self.stage+=1;self.frame=0
  if self.stage>=len(STAGES):self.finish('captured_pending_visual_review');return
  name,asset,palms=STAGES[self.stage]
  for c in self.fx:c.deactivate();c.set_visibility(False,True)
  self.hero.set_actor_hidden_in_game(not palms)
  self.mesh.set_position(.30,False)
  for i,c in enumerate(self.fx):
   c.detach_from_component(unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,False)
   if not palms and i==1:continue
   c.set_asset(self.assets[name]);c.set_editor_property('allow_scalability',False);c.set_visibility(True,True)
   if palms:
    bone='hand_l' if i==0 else 'hand_r';assert self.mesh.does_socket_exist(bone)
    assert c.attach_to_component(self.mesh,bone,unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.SNAP_TO_TARGET,unreal.AttachmentRule.KEEP_WORLD,False)
    c.set_variable_float('User.HandSideSign',-1.0 if i==0 else 1.0)
    c.set_variable_float('User.CastDurationSec',.5);c.set_variable_float('User.ElapsedAtStartSec',0.0)
   else:c.set_world_location(unreal.Vector(0,0,140),False,False)
   c.set_component_tick_enabled(True);c.reinitialize_system();c.activate(True)
  if palms:self.camera(unreal.Vector(200,-350,210),unreal.Vector(0,0,120))
  self.stageStart=unreal.GameplayStatics.get_time_seconds(self.pie);self.state='run';self.report['currentStage']=name;self.persist()
 def finish(self,status):
  self.report['status']=status;self.persist()
  if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
  try:self.level.editor_request_end_play()
  except Exception:pass
  print('CASTV3_PROOF_FINISHED',status,str(OUT),flush=True)
  if '-CastFlightV3CaptureOnce' in unreal.SystemLibrary.get_command_line():unreal.SystemLibrary.quit_editor()
 def tick(self,dt):
  try:
   if time.monotonic()-self.started>120:raise TimeoutError('PIE审核超时')
   if self.state=='wait':
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    if not worlds:return
    assert len(worlds)==1 and worlds[0].get_path_name().startswith('/Temp/')
    self.pie=worlds[0];actors=unreal.GameplayStatics.get_all_actors_of_class
    
    for pawn in actors(self.pie,unreal.DefaultPawn):pawn.set_actor_hidden_in_game(True)
    self.hero=actors(self.pie,unreal.SkeletalMeshActor)[0];self.mesh=self.hero.get_component_by_class(unreal.SkeletalMeshComponent)
    fxs=sorted(actors(self.pie,unreal.NiagaraActor),key=lambda a:a.get_actor_label())
    assert len(fxs)==2;self.fx=[a.get_component_by_class(unreal.NiagaraComponent) for a in fxs]
    self.camActor=actors(self.pie,unreal.SceneCapture2D)[0];self.cap=self.camActor.get_component_by_class(unreal.SceneCaptureComponent2D)
    self.target=unreal.RenderingLibrary.create_render_target2d(self.pie,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8);self.cap.set_editor_property('texture_target',self.target)
    self.state='warm';self.last=time.monotonic();return
   if self.state=='warm':
    if time.monotonic()-self.last<1.0:return
    self.next_stage();return
   if self.stage<0 or self.stage>=len(STAGES):return
   age=unreal.GameplayStatics.get_time_seconds(self.pie)-self.stageStart
   name,asset,palms=STAGES[self.stage]
   if not palms:
    pos=unreal.Vector(age*650.,60*math.sin(age*1.5),140)
    self.fx[0].set_world_location(pos,False,False)
    self.camera(pos+unreal.Vector(180,-650,190),pos+unreal.Vector(-150,0,0))
   if self.state=='run':
    goals=[.08,.20,.32] if palms else [.16,.38,.72]
    if self.frame>=len(goals):
     if age>1.0:self.next_stage()
     return
    if age<goals[self.frame]:return
    self.cap.capture_scene();self.last=time.monotonic();self.state='flush';return
   if self.state=='flush':
    if time.monotonic()-self.last<.01:return
    self.cap.capture_scene();self.last=time.monotonic();self.state='export';return
   if self.state=='export':
    if time.monotonic()-self.last<.01:return
    path=OUT/(name+'_'+str(self.frame+1)+'.png')
    unreal.RenderingLibrary.export_render_target(self.pie,self.target,str(OUT),path.name)
    assert path.exists() and path.stat().st_size>1000
    if self.frame==2:
     evidence={'componentLocations':[str(c.get_world_location()) for c in self.fx],'palms':[str(self.mesh.get_socket_location(b)) for b in ['hand_l','hand_r']],'particles':[]}
     try:
      cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.fx[0])
      evidence['captured']=bool(unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),self.fx[0],False,1.0/60.0))
      for ei in range(cache.get_num_emitters()):
       em=str(cache.get_emitter_name(ei));positions=list(cache.read_position_attribute('Position',em,True,0));colors=list(cache.read_color_attribute('Color',em,0))
       e={'emitter':em,'count':len(positions),'positions':[str(p) for p in positions[:3]],'alpha':[float(c.a) for c in colors[:6]]}
       for attribute in ['Age','Lifetime','SubImageIndex','RibbonLinkOrder']:
        pass
        try:e[attribute]=list(cache.read_float_attribute(attribute,em,0))[:6]
        except Exception:pass
       for attribute in ['SpriteSize','UVScale','PivotOffset']:
        try:e[attribute]=[str(v) for v in cache.read_vector2_attribute(attribute,em,0)][:3]
        except Exception as ex:e[attribute+'Error']=str(ex)[:120]
       evidence['particles'].append(e)
     except Exception:evidence['error']=traceback.format_exc()[-1600:]
     self.report.setdefault('simCache',{})[name]=evidence
    self.report['frames'].append({'stage':name,'path':path.name,'timeSec':round(age,4),'active':[c.is_active() for c in self.fx],'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    self.persist();self.frame+=1;self.state='run'
  except Exception:
   self.report['errors'].append(traceback.format_exc());self.finish('failed')
try:unreal._castflight_v3inspection=CastProof()
except Exception:
 OUT.mkdir(parents=True,exist_ok=True);(OUT/'BootFailure.txt').write_text(traceback.format_exc(),encoding='utf8');print(traceback.format_exc(),flush=True)
