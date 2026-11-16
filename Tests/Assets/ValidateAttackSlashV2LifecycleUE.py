# -*- coding: utf-8 -*-
"""Monolith内运行的有限十实例原生生命周期测试；不创建播放器、不测GPU耗时。

同时运行默认、最短和最长Duration/Radius；中途取消一半，最终销毁自有Actor与回调。
"""
from pathlib import Path
import json,time,traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE=globals().get('ASSET_BASE','/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash')
SYSTEM_NAME=globals().get('SYSTEM_NAME','NS_DBA_AttackSlash_V2')
EVIDENCE=Path(globals().get('EVIDENCE_FOLDER',ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2'))
OWNER_KEY=globals().get('LIFECYCLE_OWNER_KEY','_attack_slash_v2_lifecycle')


class SlashLifecycle:
    def __init__(self):
        previous=getattr(unreal,OWNER_KEY,None)
        if previous and previous.report.get('status')=='running':raise RuntimeError('生命周期检查正在运行，拒绝重入')
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);self.owned=[];self.components=[];self.step=0;self.started=time.monotonic()
        self.report=dict(status='running',scope='Editor CPU solo native lifecycle only; no GPU/FPS or pool claims',cases=[])
        for i in range(10):
            a=self.actors.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(i*700,2000,0));a.set_actor_label('TEMP_AttackSlashLifecycle_'+str(i));self.owned.append(a)
            c=a.get_component_by_class(unreal.NiagaraComponent);c.set_asset(unreal.load_asset(BASE+'/Niagara/'+SYSTEM_NAME))
            duration=.25 if i==0 else 2. if i==1 else globals().get('DEFAULT_DURATION',.9);radius=100. if i==0 else 500. if i==1 else 230.
            c.set_variable_float('User.Duration',duration);c.set_variable_float('User.Radius',radius)
            c.set_editor_property('allow_scalability',False);c.set_force_solo(True);c.reinitialize_system();c.set_component_tick_enabled(False)
            self.components.append(c);self.report['cases'].append(dict(index=i,duration=duration,radius=radius,cancelled=i>=5))
        self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist()

    def persist(self):
        (EVIDENCE/'Lifecycle.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')

    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        for a in self.owned:self.actors.destroy_actor(a)
        self.report['temporaryActorsRemoved']=True;self.persist()

    def tick(self,delta):
        try:
            if time.monotonic()-self.started>90:raise RuntimeError('有限生命周期测试超时')
            for c in self.components:c.advance_simulation(1,1/60)
            self.step+=1
            if self.step==7:
                for row,c in zip(self.report['cases'],self.components):
                    cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
                    valid=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),c,False,1/60);assert valid
                    colors=list(cache.read_color_attribute('Color',str(cache.get_emitter_name(0)),0))
                    row.update(particlesAtStart=len(colors),maxAlphaAtStart=max((v.a for v in colors),default=0),activeAtStart=c.is_active())
                    assert row['particlesAtStart']==1 and row['maxAlphaAtStart']>.99 and row['activeAtStart']
            if self.step==20:
                for c in self.components[5:]:c.deactivate()
            if self.step==150:
                active=[i for i,c in enumerate(self.components) if c.is_active()]
                self.report.update(activeAfter2p5Seconds=active,simulatedSeconds=self.step/60)
                assert not active,'自然结束或取消后仍有活跃实例：'+str(active)
                self.report['status']='passed';self.finish()
        except Exception:
            self.report.update(status='failed',error=traceback.format_exc());self.finish()


setattr(unreal,OWNER_KEY,SlashLifecycle())
print('ATTACK_SLASH_LIFECYCLE_STARTED',BASE)
