# -*- coding: utf-8 -*-
"""审核系统的原生到期、迟到和组件取消探针；不冒充正式GAS生命周期验证。"""
import unreal as u,json,traceback
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign'
OUT=ROOT/'Saved/Validation/PalmRedesignReview20261026/Lifecycle'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
class LifecycleAudit:
 def __init__(self):
  assert not u.EditorLevelLibrary.get_pie_worlds(False)
  OUT.mkdir(parents=True,exist_ok=False)
  self.report={'status':'running','scope':'native review component only; no formal GAS or network','rows':[],'errors':[]}
  self.cases=[(t,age,late,cancel) for t in ['Fire','Ice'] for age,late,cancel in [(.1,0,False),(.25,0,False),(.4,0,False),(.5,0,False),(.6,0,False),(.1,.6,False),(.1,.2,False),(.4,.2,False),(.25,0,True)]]
  self.index=-1;self.state='wait';self.c=None;self.level=u.get_editor_subsystem(u.LevelEditorSubsystem);self.handle=u.register_slate_post_tick_callback(self.tick);self.level.editor_request_begin_play()
 def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,indent=2,default=str),encoding='utf8')
 def next(self):
  if self.c:self.c.destroy_component(self.c);self.c=None
  self.index+=1
  if self.index>=len(self.cases):self.finish('verified');return
  typ,age,late,cancel=self.cases[self.index]
  self.c=u.NiagaraFunctionLibrary.spawn_system_at_location(self.pie,u.load_asset(BASE+'/Niagara/NS_DBA_'+typ+'PalmCast_V3_ConceptReview'),u.Vector(0,40,110),auto_destroy=False,auto_activate=False,pre_cull_check=False)
  c=self.c;c.set_force_solo(True);c.set_editor_property('allow_scalability',False);c.set_visibility(True,True)
  c.set_variable_float('User.CastDurationSec',.5);c.set_variable_float('User.ElapsedAtStartSec',late)
  c.set_age_update_mode(u.NiagaraAgeUpdateMode.DESIRED_AGE);c.set_seek_delta(1/60);c.set_desired_age(age);c.activate(True)
  self.state='settle';self.ticks=0
 def sample(self):
  typ,age,late,cancel=self.cases[self.index];c=self.c
  cache=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
  captured=bool(u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,u.NiagaraSimCacheCreateParameters(),c,False,1/60))
  counts=[len(cache.read_position_attribute('Position',str(cache.get_emitter_name(i)),True,0)) for i in range(cache.get_num_emitters())] if captured else None
  row={'type':typ,'desiredAge':age,'elapsedAtStart':late,'cancelComponent':cancel,'cancelMode':'deactivate then allow lifetime to drain; not immediate kill' if cancel else None,'visibilityNeverHidden':True,'isActive':c.is_active(),'cacheCaptured':captured,'actualCounts':counts}
  # .5处采用原生固定步进边界观测，不把出生帧偏移误记为失败；.6必须结束。
  should_end=cancel or age+late>=.6
  row['ended']=not c.is_active() or (counts is not None and sum(counts)==0)
  if should_end:assert row['ended'],row
  elif age+late<.49:assert captured and sum(counts)>0,row
  self.report['rows'].append(row);self.persist();self.next()
 def tick(self,dt):
  try:
   if self.state=='wait':
    ws=u.EditorLevelLibrary.get_pie_worlds(False)
    if not ws:return
    self.pie=ws[0]
    for a in u.GameplayStatics.get_all_actors_of_class(self.pie,u.NiagaraActor):a.get_component_by_class(u.NiagaraComponent).deactivate()
    self.next();return
   self.ticks+=1
   if self.state=='settle' and self.ticks>=25:
    if self.cases[self.index][3]:
     self.c.set_age_update_mode(u.NiagaraAgeUpdateMode.TICK_DELTA_TIME);self.c.deactivate();self.cancel_time=u.GameplayStatics.get_time_seconds(self.pie);self.state='cancel';self.ticks=0
    else:self.sample()
   elif self.state=='cancel' and u.GameplayStatics.get_time_seconds(self.pie)-self.cancel_time>=.65:self.sample()
  except Exception:self.report['errors'].append(traceback.format_exc());self.finish('failed')
 def finish(self,status):
  self.report['status']=status
  if self.handle:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
  if self.c:self.c.destroy_component(self.c)
  self.persist();self.level.editor_request_end_play();print('PALM_LIFECYCLE',str(OUT),status)
u._palm_lifecycle=LifecycleAudit()
