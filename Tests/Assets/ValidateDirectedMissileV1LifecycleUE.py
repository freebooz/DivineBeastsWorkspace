# -*- coding: utf-8 -*-
"""有限原生十实例验证：真实历史Ribbon位置、粒子类型、Deactivate及最终清理。"""
from pathlib import Path
import json,time,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1';BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile'
class MissileLifecycle:
    def __init__(self):
        prior=getattr(unreal,'_missile_v1_lifecycle',None);assert not prior or prior.report['status']!='running'
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);self.owned=[];self.components=[];self.starts=[];self.step=0;self.handle=None;self.started=time.monotonic();self.report=dict(status='running',cases=[],performanceMeasured=False,scope='native editor CPU history and finite lifecycle only')
        try:
            for i in range(10):
                start=unreal.Vector(i*600,3500,0);actor=self.actors.spawn_actor_from_class(unreal.NiagaraActor,start);actor.set_actor_label('TEMP_DirectedMissileLife_'+str(i));self.owned.append(actor);c=actor.get_component_by_class(unreal.NiagaraComponent);c.set_asset(unreal.load_asset(BASE+'/Niagara/NS_DBA_DirectedMissile_V1'));c.set_editor_property('allow_scalability',False);c.set_force_solo(True);c.set_variable_vec3('User.Direction',unreal.Vector(1,0,0));c.reinitialize_system();c.set_component_tick_enabled(False);self.components.append(c);self.starts.append(start)
            self.handle=unreal.register_slate_post_tick_callback(self.batch_tick);self.persist()
        except Exception:
            for actor in self.owned:self.actors.destroy_actor(actor)
            raise
    def persist(self):(OUT/'Lifecycle.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
    def finish(self):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        for a in self.owned:self.actors.destroy_actor(a)
        self.report['temporaryActorsRemoved']=True;self.persist()
    def batch_tick(self,dt):
        # 后台编辑器Slate频率不代表Niagara性能；仍逐1/60秒推进，只批处理有限审核步骤。
        for _ in range(4):
            if self.handle:self.tick(dt)
    def tick(self,dt):
        try:
            if time.monotonic()-self.started>120:raise RuntimeError('有限十实例验证超时')
            self.step+=1
            for c,start in zip(self.components,self.starts):c.set_world_location(start+unreal.Vector(min(self.step,45)*20,0,0),False,False);c.advance_simulation(1,1/60)
            if self.step==35:
                for index,c in enumerate(self.components):
                    cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c);assert unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),c,False,1/60)
                    counts={};spans={}
                    for i in range(cache.get_num_emitters()):
                        name=str(cache.get_emitter_name(i));points=list(cache.read_position_attribute('Position',name,True,0));counts[name]=len(points)
                        if name!='MissileHead':
                            span=max(p.x for p in points)-min(p.x for p in points);spans[name]=span;assert len(points)>20 and span>350,'Ribbon历史位置未保留或未随组件运动出生'
                    assert counts['MissileHead']==1 and set(counts)=={'MissileHead','GoldTrail','AmberTrail','SoftGoldTrail'}
                    self.report['cases'].append(dict(index=index,countsAt35Steps=counts,historySpansCm=spans,cancelled=index>=5))
                for c in self.components[5:]:c.deactivate()
            if self.step==150:
                active=[i for i,c in enumerate(self.components) if c.is_active()];self.report['activeAfter2p5Seconds']=active;assert not active
                self.report.update(status='passed',simulatedSeconds=2.5);self.finish()
        except Exception:self.report.update(status='failed',error=traceback.format_exc());self.finish()
unreal._missile_v1_lifecycle=MissileLifecycle();print('MISSILE_TEN_INSTANCE_HISTORY_STARTED')
