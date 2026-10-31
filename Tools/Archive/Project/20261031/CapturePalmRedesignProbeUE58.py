# -*- coding: utf-8 -*-
"""Monolith内真实PIE掌心审核：固定相机，手动固定步进Niagara，记录截图与真实粒子。
只清理本任务拥有的临时审核地图，原V3编译脏缓存不保存。该工具不绑定正式技能。
"""
raise RuntimeError('SCRIPT_RETIRED: 已退休归档，见Tools/Archive/Project/20261031/README.md。')
import unreal as u,time,json,traceback,os
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign'
OLD='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight'
OUT=ROOT/'Saved/Validation/PalmRedesignReview20261026/Native'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')

class PalmReview:
 def __init__(self):
  assert u.SystemLibrary.get_engine_version().startswith('5.8')
  assert not u.EditorLevelLibrary.get_pie_worlds(False),'不干扰已有PIE'
  self.ed=u.get_editor_subsystem(u.EditorActorSubsystem);self.level=u.get_editor_subsystem(u.LevelEditorSubsystem)
  actors=self.ed.get_all_level_actors()
  dirty=u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
  # Engine/Maps/Entry启动时已有这六个模板环境Actor；只接受已核对的精确白名单。
  entry_defaults={'PlayerStart-1','SkyLight','DirectionalLight','SkyAtmosphere','VolumetricCloud','ExponentialHeightFog'}
  if dirty:assert all(a.get_actor_label() in entry_defaults or a.get_actor_label().startswith(('CastV3','PalmReview','WorldSettings','WorldDataLayers','Brush','DefaultPhysicsVolume')) for a in actors),'非本轮未保存地图'
  # Python可能仍持有旧World；在本任务审核World内替换自有演员，不销毁World触发GC泄漏。
  self.world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
  assert self.world.get_path_name().startswith(('/Temp/','/Engine/Maps/Entry',BASE+'/Maps/')),'非审核World'
  for a in actors:
   if a.get_actor_label().startswith(('CastV3','PalmReview')) or a.get_actor_label() in {'SkyLight','DirectionalLight','VolumetricCloud','ExponentialHeightFog'}:self.ed.destroy_actor(a)
  actors=[];OUT.mkdir(parents=True,exist_ok=False)
  self.report={'engine':u.SystemLibrary.get_engine_version(),'pid':os.getpid(),'render':'UE5.8 D3D12 PIE fixed-step','status':'running','manualAccepted':False,'frames':[],'errors':[],'simCache':{}}
  def spawn(cls,label,pos):
   a=self.ed.spawn_actor_from_class(cls,u.Vector(*pos));a.set_actor_label('PalmReview'+label);return a
  floor=spawn(u.StaticMeshActor,'Floor',(0,0,0));floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(u.Vector(20,20,1))
  for name,yaw,pitch,power in [('Key',-35,-45,5),('Fill',140,-30,2)]:
   a=spawn(u.DirectionalLight,name,(0,0,300));a.set_actor_rotation(u.Rotator(pitch=pitch,yaw=yaw),False);a.light_component.set_editor_property('intensity',power)
  a=spawn(u.SkyLight,'Sky',(0,0,250));a.light_component.set_editor_property('intensity',1.0)
  fx=spawn(u.NiagaraActor,'Fx',(0,0,80));fx.get_component_by_class(u.NiagaraComponent).set_auto_activate(False)
  cam=spawn(u.SceneCapture2D,'Camera',(75,-110,120));cam.set_actor_rotation(u.MathLibrary.find_look_at_rotation(cam.get_actor_location(),u.Vector(0,0,92)),False)
  cap=cam.get_component_by_class(u.SceneCaptureComponent2D);cap.set_editor_property('capture_every_frame',False);cap.set_editor_property('capture_on_movement',False);cap.set_editor_property('capture_source',u.SceneCaptureSource.SCS_FINAL_COLOR_LDR);cap.set_editor_property('fov_angle',35.)
  pp=cap.get_editor_property('post_process_settings')
  for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':u.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':0.0,'override_bloom_intensity':True,'bloom_intensity':.15,'override_motion_blur_amount':True,'motion_blur_amount':0.0}.items():pp.set_editor_property(k,v)
  cap.set_editor_property('post_process_settings',pp)
  self.world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
  self.stages=[('OldIce',OLD+'/Niagara/NS_DBA_IcePalmCast_V3'),('Fire',BASE+'/Niagara/NS_DBA_FirePalmCast_V3_ConceptReview'),('Ice',BASE+'/Niagara/NS_DBA_IcePalmCast_V3_ConceptReview')]
  self.stage=-1;self.state='wait';self.start=time.monotonic();self.ticks=0;self.handle=u.register_slate_post_tick_callback(self.tick)
  self.level.editor_request_begin_play()
 def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
 def begin_stage(self):
  self.stage+=1
  if self.stage>=len(self.stages):self.finish('captured');return
  name,path=self.stages[self.stage];self.c.deactivate();self.c.set_asset(u.load_asset(path));self.c.set_auto_activate(False)
  self.c.set_force_solo(True);self.c.set_editor_property('allow_scalability',False)
  self.c.set_variable_float('User.CastDurationSec',.5);self.c.set_variable_float('User.ElapsedAtStartSec',0)
  self.c.set_variable_float('User.HandSideSign',1);self.c.set_paused(False);self.c.activate(True)
  self.c.set_component_tick_enabled(True)
  self.state='arm';self.ticks=0
 def cache(self,name):
  cache=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.c)
  ok=bool(u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,u.NiagaraSimCacheCreateParameters(),self.c,False,1/60))
  rows=[]
  if ok:
   for i in range(cache.get_num_emitters()):
    n=str(cache.get_emitter_name(i));p=list(cache.read_position_attribute('Position',n,True,0));colors=list(cache.read_color_attribute('Color',n,0))
    rows.append({'emitter':n,'count':len(p),'positions':[str(x) for x in p[:3]],'colors':[str(x) for x in colors[:3]]})
  self.report['simCache'][name]={'captured':ok,'emitters':rows}
 def tick(self,dt):
  try:
   assert time.monotonic()-self.start<180,'超时'
   if self.state=='wait':
    ws=u.EditorLevelLibrary.get_pie_worlds(False)
    if not ws:return
    self.pie=ws[0];get=u.GameplayStatics.get_all_actors_of_class
    for a in get(self.pie,u.DefaultPawn):a.set_actor_hidden_in_game(True)
    self.c=get(self.pie,u.NiagaraActor)[0].get_component_by_class(u.NiagaraComponent)
    self.cap=get(self.pie,u.SceneCapture2D)[0].get_component_by_class(u.SceneCaptureComponent2D)
    self.rt=u.RenderingLibrary.create_render_target2d(self.pie,1920,1080,u.TextureRenderTargetFormat.RTF_RGBA8);self.cap.set_editor_property('texture_target',self.rt)
    self.state='warm';self.ticks=0;return
   self.ticks+=1
   if self.state=='warm':
    if self.ticks>30:self.begin_stage()
   elif self.state=='arm' and self.ticks==2:
    self.c.set_component_tick_enabled(False)
    print('PALM_ACTIVE_BEFORE',self.c.is_active())
    self.c.set_paused(False);self.c.advance_simulation(10,1/60)
    print('PALM_ACTIVE_AFTER',self.c.is_active())
    self.cache(self.stages[self.stage][0]);self.state='capture';self.ticks=0
   elif self.state=='capture':
    self.cap.capture_scene()
    if self.ticks>8:self.state='export';self.ticks=0
   elif self.state=='export' and self.ticks>2:
    name=self.stages[self.stage][0];p=OUT/(name+'.png');u.RenderingLibrary.export_render_target(self.pie,self.rt,str(OUT),p.name)
    assert p.exists() and p.stat().st_size>1000
    self.report['frames'].append(str(p));self.persist();self.begin_stage()
  except Exception:self.report['errors'].append(traceback.format_exc());self.finish('failed')
 def finish(self,status):
  self.report['status']=status;self.persist()
  if self.handle:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
  self.level.editor_request_end_play();print('PALM_REVIEW_FINISH',str(OUT),status,flush=True)

u._palm_review=PalmReview()
