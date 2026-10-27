# -*- coding: utf-8 -*-
"""审核专用质量/十组双掌并发实测。不连接正式Catalog，不冒充十人联网或打包性能。"""
import unreal as u,time,json,traceback,runpy,shutil
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign'
OUT=ROOT/'Saved/Validation/PalmRedesignReview20261026/Performance'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
QUALITY={'Epic':(12,14,1,1.0),'High':(8,10,1,1.0),'Medium':(4,5,0,0.),'Low':(2,2,0,0.)}
class PalmAudit:
 def __init__(self):
  assert not u.EditorLevelLibrary.get_pie_worlds(False)
  self.world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();assert self.world.get_path_name().startswith(BASE+'/Maps/')
  OUT.mkdir(parents=True,exist_ok=False);self.report={'status':'running','context':'UE editor PIE + 1920x1080 SceneCapture; synthetic concurrent casters; no network','qualityMechanism':'explicit User count parameters; formal quality adapter not installed','stages':[],'errors':[]}
  self.assets={t:u.load_asset(BASE+'/Niagara/NS_DBA_'+t+'PalmCast_V3_ConceptReview') for t in ['Fire','Ice']}
  self.stages=[('Fire','High',0)]+[(t,q,2) for t in ['Fire','Ice'] for q in QUALITY]+[(t,'High',20) for t in ['Fire','Ice']]
  self.stage=-1;self.state='wait';self.ticks=0;self.fx=[];self.level=u.get_editor_subsystem(u.LevelEditorSubsystem);self.handle=u.register_slate_post_tick_callback(self.tick);self.level.editor_request_begin_play()
 def cmd(self,s):u.SystemLibrary.execute_console_command(self.pie,s)
 def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,indent=2),encoding='utf8')
 def next_stage(self):
  for c in self.fx:c.destroy_component(c)
  self.fx=[];self.stage+=1
  if self.stage>=len(self.stages):self.finish('measured');return
  typ,q,count=self.stages[self.stage];detail=QUALITY[q][0 if typ=='Fire' else 1];ripple=QUALITY[q][2]
  for i in range(count):
   x=(i//2%5-2)*42+(i%2*14-7) if count>2 else (i*46-23)
   c=u.NiagaraFunctionLibrary.spawn_system_at_location(self.pie,self.assets[typ],u.Vector(x,20+(i//10)*48,112),auto_destroy=False,auto_activate=False,pre_cull_check=False)
   assert c;c.set_editor_property('allow_scalability',False)
   for k,v in {'CastDurationSec':.5,'ElapsedAtStartSec':0.,'HandSideSign':1 if i%2 else -1,'AirRippleScale':QUALITY[q][3]}.items():c.set_variable_float('User.'+k,v)
   for k,v in {'CoreCount':1,'BodyCount':3 if typ=='Fire' else 6,'DetailCount':detail,'RippleCount':ripple}.items():c.set_variable_int('User.'+k,v)
   c.activate(True);self.fx.append(c)
  filename=f'Palm_{OUT.name}_{typ}_{q}_{count}.csv'
  self.row={'type':typ,'quality':q,'systemsRequested':count,'expectedCountsPerPalm':[1,3 if typ=='Fire' else 6,detail,ripple],'actualCounts':None,'csv':str(OUT/filename),'csvSource':str(ROOT/'Game/Saved/Profiling/CSV'/filename)}
  self.report['stages'].append(self.row);self.phase_start=u.GameplayStatics.get_time_seconds(self.pie);self.state='warm';self.ticks=0;self.shot_pending=False
 def sample(self):
  if not self.fx:return
  allcounts=[];positions=[]
  for c in self.fx:
   cache=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
   assert u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,u.NiagaraSimCacheCreateParameters(),c,False,1/60)
   counts=[len(cache.read_position_attribute('Position',str(cache.get_emitter_name(i)),True,0)) for i in range(cache.get_num_emitters())]
   assert counts==self.row['expectedCountsPerPalm'],(counts,self.row)
   allcounts.append(counts);positions.append(str(c.get_world_location()))
  self.row['actualCounts']=allcounts[0];self.row['allSystemCounts']=allcounts;self.row['positions']=positions
  self.row['activeSystemsVerified']=len(allcounts);self.shot_pending=True
 def tick(self,dt):
  try:
   if self.state=='wait':
    ws=u.EditorLevelLibrary.get_pie_worlds(False)
    if not ws:return
    self.pie=ws[0]
    for a in u.GameplayStatics.get_all_actors_of_class(self.pie,u.NiagaraActor):a.get_component_by_class(u.NiagaraComponent).deactivate()
    for a in u.GameplayStatics.get_all_actors_of_class(self.pie,u.DefaultPawn):a.set_actor_hidden_in_game(True)
    camera=u.GameplayStatics.get_all_actors_of_class(self.pie,u.SceneCapture2D)[0]
    eye=u.Vector(40,365,205);target=u.Vector(0,40,124)
    camera.set_actor_location(eye,False,False);camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(eye,target),False)
    self.report['camera']={'eye':str(eye),'target':str(target),'rotation':str(camera.get_actor_rotation()),'resolution':[1920,1080],'fov':43}
    cap=camera.get_component_by_class(u.SceneCaptureComponent2D);self.cap=cap;cap.set_editor_property('fov_angle',43.)
    self.rt=u.RenderingLibrary.create_render_target2d(self.pie,1920,1080,u.TextureRenderTargetFormat.RTF_RGBA8);cap.set_editor_property('texture_target',self.rt);cap.set_editor_property('capture_every_frame',True)
    self.cmd('r.GPUCsvStatsEnabled 1');self.cmd('csvcategory Particles enable');self.cmd('t.MaxFPS 0');self.next_stage();return
   self.ticks+=1;elapsed=u.GameplayStatics.get_time_seconds(self.pie)-self.phase_start
   if self.shot_pending and self.state=='warm':
    filename=f"{self.row['type']}_{self.row['quality']}_{self.row['systemsRequested']}_visible.png"
    u.RenderingLibrary.export_render_target(self.pie,self.rt,str(OUT),filename);self.row['nativeVisibilityScreenshot']=str(OUT/filename);self.shot_pending=False
   if elapsed>=.65:
    for c in self.fx:c.activate(True)
    self.phase_start=u.GameplayStatics.get_time_seconds(self.pie)
   elif .15<elapsed<.35 and self.row['actualCounts'] is None:self.sample()
   if self.state=='warm' and self.ticks>=90:
    self.cmd('csvprofile STARTFILE='+Path(self.row['csvSource']).name);self.cmd('csvprofile FRAMES=300');self.state='record';self.ticks=0
   elif self.state=='record' and self.ticks>=330:
    src=Path(self.row['csvSource']);assert src.exists() and src.stat().st_size>500,'CSV原生输出缺失'
    shutil.copyfile(src,self.row['csv']);self.persist();self.next_stage()
  except Exception:self.report['errors'].append(traceback.format_exc());self.finish('failed')
 def finish(self,status):
  self.report['status']=status
  if self.handle:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
  for c in self.fx:
   if u.SystemLibrary.is_valid(c):c.destroy_component(c)
  self.report['remainingOwnedComponents']=sum(1 for c in self.fx if u.SystemLibrary.is_valid(c) and not c.is_being_destroyed())
  self.persist()
  self.level.editor_request_end_play();print('PALM_AUDIT',str(OUT),status)
u._palm_audit=PalmAudit()
