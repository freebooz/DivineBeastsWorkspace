# -*- coding: utf-8 -*-
"""《神兽联盟》火球V3 P1：UE5.8 PIE独立瞬时视觉采集。
在真实游戏世界顶部生成不参与Gameplay的临时Mannequin、Niagara、相机及中性底板；
不保存原地图、不修改GAS、无权威碰撞、不申请用户账号，完成后自动销毁所有自有Actor。
"""
import unreal,time,json,shutil,traceback
from pathlib import Path
from datetime import datetime,timezone

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OUT=ROOT/'Saved/Validation/FireballV3P1'/('LivePIE_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
ASSET='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review'
KEYS={7:'01_HandsIgnite',15:'02_PalmsCharge',29:'03_StreamConverge',41:'04_CoreFormed'}

class Review:
 def __init__(self):
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  assert len(worlds)==1,'需要唯一PIE游戏世界'
  self.world=worlds[0]
  assert self.world.get_name() in ('L_Village_Start','Untitled'),'拒绝在未经批准的地图中演示'
  self.actor_list=[];self.handle=None
  self.g=unreal.get_default_object(unreal.GameplayStatics)
  self.base=unreal.Vector(0,0,4400)
  def pos(x,y,z):return self.base+unreal.Vector(x,y,z)
  self.pos=pos
  def spawn(cls,off):
   trans=unreal.Transform(location=pos(*off))
   actor=self.g.call_method('BeginDeferredActorSpawnFromClass',args=(self.world,cls.static_class(),trans,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
   assert actor,'审核Actor创建失败'
   self.g.call_method('FinishSpawningActor',args=(actor,trans,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
   self.actor_list.append(actor);actor.set_actor_enable_collision(False);return actor
  self.floor=spawn(unreal.StaticMeshActor,(0,0,-5))
  self.floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
  self.floor.set_actor_scale3d(unreal.Vector(13,13,1))
  self.model=spawn(unreal.SkeletalMeshActor,(0,0,0))
  sk=self.model.get_component_by_class(unreal.SkeletalMeshComponent)
  sk.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
  seq=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast')
  sk.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);sk.set_animation(seq)
  self.mesh=sk
  sys=unreal.load_asset(ASSET);assert isinstance(sys,unreal.NiagaraSystem)
  self.comp=unreal.NiagaraFunctionLibrary.spawn_system_at_location(self.world,sys,pos(0,0,147),auto_destroy=False,auto_activate=True,pre_cull_check=False)
  assert self.comp and self.comp.is_active(),'新资产GPU无法激活，禁止伪造截图'
  self.comp.set_editor_property('allow_scalability',False);self.comp.set_force_solo(True)
  self.comp.set_component_tick_enabled(False)
  self.eye=pos(320,-445,210);self.aim=pos(36,0,125)
  self.cam=spawn(unreal.SceneCapture2D,(320,-445,210))
  self.cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(self.eye,self.aim),False)
  self.cap=self.cam.get_component_by_class(unreal.SceneCaptureComponent2D)
  self.cap.set_editor_property('capture_every_frame',False);self.cap.set_editor_property('capture_on_movement',False)
  self.cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
  self.cap.set_editor_property('fov_angle',52)
  pp=self.cap.get_editor_property('post_process_settings')
  for key,val in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':1.25,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_motion_blur_amount':True,'motion_blur_amount':0.0,'override_bloom_intensity':True,'bloom_intensity':0.19}.items():
   pp.set_editor_property(key,val)
  self.cap.set_editor_property('post_process_settings',pp)
  self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
  self.cap.set_editor_property('texture_target',self.target)
  OUT.mkdir(parents=True,exist_ok=False)
  self.report={'status':'running','source':'UE5.8真实PIE世界Niagara逐帧SceneCapture','map':self.world.get_path_name(),
   'asset':ASSET,'productionChanged':False,'manualAccepted':False,'worldRunOnly':True,'frames':[],'error':None}
  self.samples=list(range(1,45,2));self.index=0;self.steps=0;self.warm=0;self.phase='warm';self.since=time.monotonic();self.start=self.since
  self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist()
  print('SSLM_P1_LIVE_PIE_STARTED',str(OUT))
 def persist(self):
  (OUT/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
 def finish(self,state):
  if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
  self.report['status']=state;self.report['elapsedSeconds']=time.monotonic()-self.start
  self.persist()
  if self.comp and unreal.SystemLibrary.is_valid(self.comp):
   self.comp.destroy_component(self.comp)
  for actor in reversed(self.actor_list):
   if unreal.SystemLibrary.is_valid(actor):actor.call_method('K2_DestroyActor')
  print('SSLM_P1_LIVE_PIE_END',state,str(OUT))
 def tick(self,dt):
  try:
   if time.monotonic()-self.start>140:raise RuntimeError('审核采样超时')
   if self.phase=='warm':
    self.warm+=1
    if self.warm<30:return
    self.phase='step'
   if self.phase=='step':
    if self.steps<self.samples[self.index]:
     self.comp.advance_simulation(1,1/60);self.steps+=1;self.mesh.set_position(min(0.69,self.steps/60),False)
     return
    self.cap.capture_scene();self.since=time.monotonic();self.phase='capture2';return
   if time.monotonic()-self.since<.13:return
   if self.phase=='capture2':
    self.cap.capture_scene();self.since=time.monotonic();self.phase='export';return
   fn='Frame_'+str(self.index).zfill(3)+'.png'
   unreal.RenderingLibrary.export_render_target(self.world,self.target,str(OUT),fn)
   p=OUT/fn
   if not p.is_file() or p.stat().st_size<3500:raise RuntimeError('真实GPU帧导出失败: '+fn)
   key=KEYS.get(self.steps)
   if key:shutil.copyfile(p,OUT/(key+'.png'))
   self.report['frames'].append({'index':self.index,'timeSeconds':round(self.steps/60,4),'file':str(p),'bytes':p.stat().st_size,'key':key})
   self.persist();self.index+=1
   if self.index==len(self.samples):self.finish('captured')
   else:self.phase='step'
  except Exception:
   self.report['error']=traceback.format_exc();self.finish('failed')

unreal._ssml_p1_livepie=Review()
