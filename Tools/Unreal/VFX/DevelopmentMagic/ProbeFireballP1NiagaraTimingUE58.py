# -*- coding: utf-8 -*-
"""SSLM P1: real PIE Niagara age diagnosis; temporary system only, no game asset modification."""
import unreal, json, time, traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OUT=ROOT/'Saved/Validation/FireballV3P1/PIE_TimingDiagnostics.json'
ASSET='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review'
class Probe:
    def __init__(self):
        worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
        assert len(worlds)==1
        self.world=worlds[0]
        self.comp=unreal.NiagaraFunctionLibrary.spawn_system_at_location(
            self.world,unreal.load_asset(ASSET),unreal.Vector(0,0,65000),
            auto_destroy=False,auto_activate=True,pre_cull_check=False)
        assert self.comp
        self.comp.set_editor_property('allow_scalability',False)
        self.comp.set_force_solo(True)
        self.comp.set_component_tick_enabled(True)
        self.comp.activate(True)
        self.report={'status':'running','world':self.world.get_path_name(),'initialActive':self.comp.is_active(),'frames':[]}
        self.started=time.monotonic()
        self.sample_times=[0.06,0.18,0.34,0.54,0.74,0.98]
        self.index=0
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        print('P1_TIMING_STARTED',self.report['initialActive'])
    def persist(self):
        OUT.parent.mkdir(parents=True,exist_ok=True)
        OUT.write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
    def stop(self,state):
        if self.handle:
            unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=state
        self.persist()
        if self.comp and unreal.SystemLibrary.is_valid(self.comp):
            self.comp.destroy_component(self.comp)
        print('P1_TIMING_COMPLETED',state,self.report['frames'])
    def tick(self,dt):
        try:
            elapsed=time.monotonic()-self.started
            if elapsed<self.sample_times[self.index]:return
            record={'elapsedSeconds':elapsed,'active':self.comp.is_active(),
                    'tickEnabled':self.comp.is_component_tick_enabled(),
                    'worldDeltaTime':unreal.GameplayStatics.get_world_delta_seconds(self.world),
                    'emitters':{}}
            cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.comp)
            ok=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                cache,unreal.NiagaraSimCacheCreateParameters(),self.comp,False,1/60)
            record['simCacheCaptured']=bool(ok)
            if ok:
                for i in range(cache.get_num_emitters()):
                    name=str(cache.get_emitter_name(i))
                    colors=list(cache.read_color_attribute('Color',name,0))
                    ages=list(cache.read_float_attribute('Age',name,0))
                    sizes=list(cache.read_vector2_attribute('SpriteSize',name,0))
                    record['emitters'][name]={'count':len(colors),
                        'maxAge':max(ages) if ages else 0,
                        'maxAlpha':max([x.a for x in colors],default=0),
                        'maxWidth':max([x.x for x in sizes],default=0)}
            self.report['frames'].append(record);self.persist();self.index+=1
            if self.index==len(self.sample_times):self.stop('complete')
        except Exception:
            self.report['error']=traceback.format_exc();self.stop('failed')
unreal._p1timingprobe=Probe()
