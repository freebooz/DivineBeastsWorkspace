# -*- coding: utf-8 -*-
"""独占编辑器中的2粒子真实Array DI验证；只建临时Actor，不保存原地图，不用CPU数组冒充GPU证据。"""
import unreal,json,time,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OUT=ROOT/'Saved/Validation/Fauna/ArrayProbe.json'
class FaunaArrayProbe:
    def __init__(self):
        assert '-FaunaOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
        self.started=time.monotonic();self.phase=0;self.step=0;self.mark=self.started;self.last=self.started
        self.report={'status':'running','source':'UE5.8 GPU NiagaraSimCache','cases':[],'errors':[]}
        self.api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.actor=self.api.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,0,800))
        self.actor.set_actor_label('FaunaArrayProbe_Transient')
        self.c=self.actor.get_component_by_class(unreal.NiagaraComponent)
        self.c.set_auto_activate(False)
        self.c.set_asset(unreal.load_asset('/GamePlatformVFX/Environment/Fauna/Niagara/NS_GP_Fauna_Base'))
        self.c.set_editor_property('allow_scalability',False);self.c.set_force_solo(True)
        self.c.set_variable_int('User.FaunaCount',2)
        self.positions=[unreal.Vector(10,20,30),unreal.Vector(-40,50,60)]
        self.upload();self.c.activate(True);self.c.set_component_tick_enabled(False)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
    def upload(self):
        api=unreal.NiagaraDataInterfaceArrayFunctionLibrary
        api.set_niagara_array_vector(self.c,'User.FaunaPositions',self.positions)
        api.set_niagara_array_quat(self.c,'User.FaunaOrientations',[unreal.Quat(0,0,0,1),unreal.Quat(0,0,0.70710678,0.70710678)])
        api.set_niagara_array_vector4(self.c,'User.FaunaMotionStates',[unreal.Vector4(0.2,1,1,0),unreal.Vector4(0.7,0,0,2)])
    def persist(self):
        OUT.parent.mkdir(parents=True,exist_ok=True)
        OUT.write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
    def tick(self,delta):
        try:
            now=time.monotonic()
            if now-self.started>45: raise RuntimeError('数组GPU探针超时')
            if now-self.last>=1/30:
                self.c.advance_simulation(1,min(now-self.last,0.1));self.last=now
            if self.phase==0 and now-self.mark>2:
                self.cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.c)
                self.capture=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                    self.cache,unreal.NiagaraSimCacheCreateParameters(),self.c,False,1/30)
                self.phase=1;self.mark=now
            elif self.phase==1 and now-self.mark>0.6:
                actual=list(self.cache.read_position_attribute('Position','Fauna',False,0)) if self.cache.get_num_frames() else []
                observed=sorted([(round(p.x,3),round(p.y,3),round(p.z,3)) for p in actual])
                expected=sorted([(p.x,p.y,p.z) for p in self.positions])
                passed=len(actual)==2 and all(sum((a-b)**2 for a,b in zip(p,q))<0.01 for p,q in zip(observed,expected))
                self.report['cases'].append({'step':self.step,'expected':expected,'gpuObserved':observed,'passed':passed})
                self.persist()
                if not passed: raise RuntimeError('数组上传与GPU粒子槽位不一致')
                if self.step==0:
                    self.positions=[unreal.Vector(70,-80,90),unreal.Vector(-100,110,-120)]
                    self.upload();self.step=1;self.phase=0;self.mark=now
                else: self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc());self.finish('failed')
    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.api.destroy_actor(self.actor)
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist();print('FAUNA_ARRAY_PROBE',status)
unreal._fauna_array_probe=FaunaArrayProbe()
