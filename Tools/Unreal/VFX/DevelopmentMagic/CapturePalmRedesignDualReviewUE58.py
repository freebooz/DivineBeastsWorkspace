# -*- coding: utf-8 -*-
"""UE原生双掌审核；角色骨骼约定仅属于审核Rig，不进入Niagara或正式技能。"""
import unreal as u,time,json,traceback,os,runpy
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign'
OUT=ROOT/'Saved/Validation/PalmRedesignReview20261026/Native'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
def vec(v):return [float(v.x),float(v.y),float(v.z)]
class DualReview:
 def __init__(self,video=False):
  self.video=video
  assert not u.EditorLevelLibrary.get_pie_worlds(False)
  self.ed=u.get_editor_subsystem(u.EditorActorSubsystem);self.level=u.get_editor_subsystem(u.LevelEditorSubsystem)
  self.world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
  assert self.world.get_path_name().startswith(('/Temp/',BASE+'/Maps/'))
  for a in self.ed.get_all_level_actors():
   if a.get_actor_label().startswith('PalmReview') or a.get_actor_label() in {'SkyLight','DirectionalLight','VolumetricCloud','ExponentialHeightFog'}:self.ed.destroy_actor(a)
  OUT.mkdir(parents=True,exist_ok=False)
  self.report={'status':'running','engine':u.SystemLibrary.get_engine_version(),'pid':os.getpid(),'source':'native PIE SceneCapture fixed age','formalSkillChanged':False,'manualAccepted':False,'samples':[],'screens':[],'errors':[]}
  self.assets={typ:u.load_asset(BASE+'/Niagara/NS_DBA_'+typ+'PalmCast_V3_ConceptReview') for typ in ['Fire','Ice']}
  self.pose=u.load_asset(BASE+'/Animations/AS_DBA_PalmConceptPose_Review');self.cast=u.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast')
  def spawn(cls,label,pos):
   a=self.ed.spawn_actor_from_class(cls,u.Vector(*pos));a.set_actor_label('PalmReview'+label);return a
  A=runpy.run_path(str(ROOT/'Tools/Unreal/VFX/DevelopmentMagic/AuthorPalmRedesignReviewUE58.py'))
  mat=A['make'](BASE+'/Materials/M_DBA_PalmReviewGround',u.Material,u.MaterialFactoryNew())
  if not u.MaterialEditingLibrary.get_num_material_expressions(mat):
   mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT);col=A['expr'](mat,u.MaterialExpressionConstant3Vector,constant=u.LinearColor(.13,.16,.19,1));u.MaterialEditingLibrary.connect_material_property(col,'',u.MaterialProperty.MP_EMISSIVE_COLOR);u.MaterialEditingLibrary.recompile_material(mat);A['save'](mat)
  floor=spawn(u.StaticMeshActor,'Floor',(0,0,-1));floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Plane'));floor.static_mesh_component.set_material(0,mat);floor.set_actor_scale3d(u.Vector(100,100,1))
  mat.set_editor_property('two_sided',True);u.MaterialEditingLibrary.recompile_material(mat);A['save'](mat)
  sky=spawn(u.StaticMeshActor,'Backdrop',(0,0,0));sky.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Sphere'));sky.static_mesh_component.set_material(0,mat);sky.set_actor_scale3d(u.Vector(150,150,150));sky.set_actor_enable_collision(False)
  sky.static_mesh_component.set_cast_shadow(False)
  for name,yaw,pitch,power in [('Key',-60,-45,8),('Fill',130,-25,5),('Rim',45,-70,3)]:
   a=spawn(u.DirectionalLight,name,(0,0,300));a.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);a.light_component.set_editor_property('intensity',power)
  hero=spawn(u.SkeletalMeshActor,'Hero',(0,0,0));mesh=hero.get_component_by_class(u.SkeletalMeshComponent)
  mesh.set_skeletal_mesh_asset(u.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'));mesh.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE);mesh.play_animation(self.pose,True);mesh.set_play_rate(0);mesh.set_editor_property('visibility_based_anim_tick_option',u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
  for i in range(2):
   c=spawn(u.NiagaraActor,'Fx'+str(i),(32 if i==0 else -32,35,112)).get_component_by_class(u.NiagaraComponent)
   c.set_asset(self.assets['Fire']);c.set_age_update_mode(u.NiagaraAgeUpdateMode.DESIRED_AGE);c.set_desired_age(.25);c.set_auto_activate(True)
  cam=spawn(u.SceneCapture2D,'Camera',(180,300,172));cap=cam.get_component_by_class(u.SceneCaptureComponent2D)
  cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(cam.get_actor_location(),u.Vector(0,18,129)),False)
  for k,v in {'capture_every_frame':False,'capture_on_movement':False,'capture_source':u.SceneCaptureSource.SCS_FINAL_COLOR_LDR,'fov_angle':35.}.items():cap.set_editor_property(k,v)
  cap.set_editor_property('always_persist_rendering_state',True)
  pp=cap.get_editor_property('post_process_settings')
  for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_bloom_intensity':True,'bloom_intensity':.15,'override_motion_blur_amount':True,'motion_blur_amount':0.}.items():pp.set_editor_property(k,v)
  cap.set_editor_property('post_process_settings',pp);self.world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
  mp=BASE+'/Maps/L_DBA_PalmRedesign_Review';A['claim'](mp);assert u.EditorLoadingAndSavingUtils.save_map(self.world,mp);self.report['map']=mp
  self.stages=[(t,v) for t in ['Fire','Ice'] for v in (['Wide','Close','Cast'] if video else ['Wide','Close','Side','Back','Cast','RippleOff','Low'])];self.stage=-1;self.state='wait';self.ticks=0;self.start=time.monotonic();self.handle=u.register_slate_post_tick_callback(self.tick);self.level.editor_request_begin_play()
 def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
 def palms(self):
  out=[]
  for i,s in enumerate(['l','r']):
   wrist=self.mesh.get_socket_location('hand_'+s);mid=self.mesh.get_socket_location('middle_01_'+s);across=self.mesh.get_socket_location('pinky_01_'+s)-self.mesh.get_socket_location('index_01_'+s);f=mid-wrist;n=f.cross(across)*(1 if s=='l' else -1);n=n/n.length();center=(wrist+mid)*.5+n*1.2
   self.fx[i].set_world_location_and_rotation(center,u.MathLibrary.make_rot_from_xz(f,n),False,False);out.append({'center':vec(center),'normal':vec(n)})
  return out
 def camera(self,view):
  if view in ['RippleOff','Low']:view='Close'
  target=self.fx[0].get_world_location()+u.Vector(0,0,12)
  eye={'Wide':u.Vector(180,300,172),'Close':target+u.Vector(47,70,24),'Side':target+u.Vector(85,0,22),'Back':target+u.Vector(0,-85,20),'Cast':u.Vector(180,300,180)}[view]
  if view in ['Wide','Cast']:target=u.Vector(0,18,129)
  self.cam.set_actor_location(eye,False,False);self.cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,target),False)
 def begin(self):
  self.stage+=1
  if self.stage>=len(self.stages):self.finish('screens_captured_pending_manual_review');return
  typ,view=self.stages[self.stage];self.mesh.play_animation(self.cast if view=='Cast' else self.pose,False);self.mesh.set_play_rate(0);self.mesh.set_position(.3 if view=='Cast' else 0,False)
  for i,c in enumerate(self.fx):
   c.deactivate();c.set_asset(self.assets[typ]);c.set_visibility(True,True);c.set_force_solo(True);c.set_editor_property('allow_scalability',False)
   for k,v in {'CastDurationSec':.5,'ElapsedAtStartSec':0,'HandSideSign':-1 if i==0 else 1,'EffectScale':1,'EffectIntensity':1,'AirRippleScale':1}.items():c.set_variable_float('User.'+k,v)
   c.set_variable_int('User.RippleCount',0 if view in ['RippleOff','Low'] else 1);c.set_variable_int('User.DetailCount',2 if view=='Low' else (8 if typ=='Fire' else 10))
   c.set_age_update_mode(u.NiagaraAgeUpdateMode.DESIRED_AGE);c.set_desired_age(.25);c.set_seek_delta(1/60);c.set_component_tick_enabled(True);c.set_paused(False);c.activate(True)
  self.state='settle';self.ticks=0
  if self.video:
   self.frame=0;self.report['fps']=60;self.report['simulationDurationSec']=.5;self.report['videoFramesPerView']=84
   (OUT/(typ+'_'+view)).mkdir(exist_ok=True);self.video_step()
 def video_step(self):
  phase=self.frame%42;typ,view=self.stages[self.stage]
  if view=='Cast':self.mesh.set_position(min(phase/30,1)*self.cast.get_play_length(),False)
  for c in self.fx:
   if phase==0:c.activate(True)
   c.set_desired_age((phase+1)/60);c.set_visibility(True,True)
  self.state='video_settle';self.ticks=0
 def sample(self):
  typ,view=self.stages[self.stage];row={'type':typ,'view':view,'requestedAge':.25,'palms':self.palms(),'components':[]}
  for c in self.fx:
   cache=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c);assert u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,u.NiagaraSimCacheCreateParameters(),c,False,1/60)
   es=[]
   for i in range(cache.get_num_emitters()):
    n=str(cache.get_emitter_name(i));ps=list(cache.read_position_attribute('Position',n,True,0));es.append({'emitter':n,'count':len(ps),'positions':[vec(p) for p in ps],'ages':list(cache.read_float_attribute('Age',n,0))})
   row['components'].append(es)
  self.report['samples'].append(row)
 def tick(self,dt):
  try:
   assert time.monotonic()-self.start<900
   if self.state=='wait':
    ws=u.EditorLevelLibrary.get_pie_worlds(False)
    if not ws:return
    self.pie=ws[0];get=u.GameplayStatics.get_all_actors_of_class
    for a in get(self.pie,u.DefaultPawn):a.set_actor_hidden_in_game(True)
    self.hero=next(a for a in get(self.pie,u.SkeletalMeshActor) if a.get_actor_label()=='PalmReviewHero');self.mesh=self.hero.get_component_by_class(u.SkeletalMeshComponent)
    self.fx=[a.get_component_by_class(u.NiagaraComponent) for a in sorted(get(self.pie,u.NiagaraActor),key=lambda a:a.get_actor_label()) if a.get_actor_label().startswith('PalmReviewFx')]
    self.cam=next(a for a in get(self.pie,u.SceneCapture2D) if a.get_actor_label()=='PalmReviewCamera');self.cap=self.cam.get_component_by_class(u.SceneCaptureComponent2D);self.rt=u.RenderingLibrary.create_render_target2d(self.pie,1920,1080,u.TextureRenderTargetFormat.RTF_RGBA8);self.cap.set_editor_property('texture_target',self.rt);self.state='warm';self.ticks=0;return
   self.ticks+=1
   if self.state=='warm' and self.ticks>30:self.begin()
   elif self.state=='settle':
    self.palms();self.camera(self.stages[self.stage][1]);self.cap.capture_scene()
    if self.ticks>=15:self.sample();self.state='export';self.ticks=0
   elif self.state=='export' and self.ticks>=2:
    typ,view=self.stages[self.stage];name=typ+'_'+view+'.png';u.RenderingLibrary.export_render_target(self.pie,self.rt,str(OUT),name);assert (OUT/name).exists();self.report['screens'].append(str(OUT/name));self.persist();self.begin()
   elif self.state=='video_settle':
    self.palms();self.camera(self.stages[self.stage][1]);self.cap.capture_scene()
    if self.ticks>=3:self.state='video_export';self.ticks=0
   elif self.state=='video_export' and self.ticks>=1:
    typ,view=self.stages[self.stage];folder=OUT/(typ+'_'+view);u.RenderingLibrary.export_render_target(self.pie,self.rt,str(folder),f'{self.frame:04d}.png')
    self.frame+=1
    if self.frame>=84:self.persist();self.begin()
    else:self.video_step()
  except Exception:self.report['errors'].append(traceback.format_exc());self.finish('failed')
 def finish(self,status):
  self.report['status']=status;self.persist()
  if self.handle:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
  self.level.editor_request_end_play();print('PALM_DUAL_REVIEW',str(OUT),status)
if __name__=='__main__':u._palm_dual=DualReview()
