#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实PIE全链路与GPU缓存验证；CPU诊断、GPU读回和人工美术验收分别记录。"""
from pathlib import Path
import json,math,time,traceback,runpy
import unreal
ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/Fauna/IntegrationReview.json'
MAP='/Game/Development/Fauna/Maps/L_DBA_FaunaReview'
ASSET='/DBAPresentationPack_Core/VFX/Environment/Fauna'
SPECIES=('ButterflyAzure','FishSilver')

class FaunaIntegrationReview:
    def __init__(self):
        assert '-FaunaOwnedIntegrationReview' in unreal.SystemLibrary.get_command_line()
        assert '-BirdOwnedIntegrationReview' in unreal.SystemLibrary.get_command_line()
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        assert self.editor.get_editor_world().get_path_name().split('.')[0]==MAP
        self.started=time.monotonic();self.mark=self.started;self.phase='boot'
        self.game=None;self.service=None;self.actor=None;self.component=None
        self.cases=[(s,n) for s in SPECIES for n in (0,1,32,128,512)]
        self.index=-1;self.cache=None;self.capture=None
        self.report={'status':'running','path':'SceneComponent -> ProjectPresentation -> DataLease -> PlatformVFX -> GPU',
                     'automaticScene':{},'gpuCases':[],'cpuBehavior':{},'errors':[],'humanArtAcceptance':'pending'}
        self.levels.editor_set_viewport_realtime(True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
    def persist(self):
        OUT.parent.mkdir(parents=True,exist_ok=True)
        OUT.write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
    def active(self):
        result=[]
        if self.game:
            for actor in unreal.GameplayStatics.get_all_actors_of_class(self.game,unreal.Actor):
                for c in actor.get_components_by_class(unreal.NiagaraComponent):
                    system=c.get_asset()
                    if system and system.get_path_name().startswith(ASSET+'/Niagara/NS_DBA_') and c.is_active():
                        result.append(c)
        return result
    def scene_components(self):
        return [c for actor in unreal.GameplayStatics.get_all_actors_of_class(self.game,unreal.Actor)
                for c in actor.get_components_by_class(unreal.DivineBeastsFaunaSceneComponent)]
    def accepted(self,result):
        return bool(result[0]) if isinstance(result,tuple) else bool(result)
    def begin_case(self):
        self.index+=1;self.cache=None;self.capture=None
        if self.index>=len(self.cases):
            self.service.stop_all_ambient_fauna()
            self.phase='final_clear';self.mark=time.monotonic();return
        species,count=self.cases[self.index]
        self.component.set_editor_properties({'species_id':species,'count':count,'seed':137})
        result=self.component.apply_fauna()
        if not self.accepted(result): raise RuntimeError('有效正式请求被拒绝：'+repr(result))
        self.phase='settle';self.mark=time.monotonic()
    def capture_gpu(self,c):
        self.cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
        self.capture=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
            self.cache,unreal.NiagaraSimCacheCreateParameters(),c,False,1.0/30.0)
    def read_gpu(self):
        if not self.cache.get_num_frames(): return [],[],[]
        return (list(self.cache.read_position_attribute('Position','Fauna',False,0)),
                list(self.cache.read_quat_attribute('MeshOrientation','Fauna',False,0)),
                list(self.cache.read_vector4_attribute('DynamicMaterialParameter','Fauna',0)))
    def tick(self,delta):
        try:
            now=time.monotonic();elapsed=now-self.mark
            if now-self.started>300: raise RuntimeError('真实群落PIE验收超时')
            if self.phase=='boot' and elapsed>2:
                self.levels.editor_request_begin_play();self.phase='world';self.mark=now
            elif self.phase=='world':
                self.game=self.editor.get_game_world()
                if not self.game: return
                services=[x for x in unreal.ObjectIterator(unreal.DivineBeastsPresentationClientSubsystem)
                          if isinstance(x.get_outer(),unreal.LocalPlayer)]
                if len(services)>1: raise RuntimeError('多个LocalPlayer，拒绝猜测验收对象')
                if not services: return
                self.service=services[0];self.phase='automatic';self.mark=now
            elif self.phase=='automatic':
                active=self.active()
                seen={c.get_asset().get_name().removeprefix('NS_DBA_') for c in active}
                if seen!=set(SPECIES) or len(active)!=2:
                    if elapsed>30: raise RuntimeError('两个场景蓝图未自动生成唯一物种：'+repr([c.get_path_name() for c in active]))
                    return
                self.report['automaticScene']={'pass':True,'world':self.game.get_path_name(),
                    'systems':[c.get_asset().get_path_name() for c in active]}
                # 使用产品诊断接口观察真实生产CPU状态，不能称为GPU证据。
                comps=self.scene_components()
                snapshots={}
                for c in comps:
                    result=c.query_fauna_state()
                    if self.accepted(result): snapshots[str(c.get_editor_property('species_id'))]=result
                butterfly=snapshots.get('ButterflyAzure')
                if not butterfly or int(butterfly[2])<1:
                    if elapsed>65: raise RuntimeError('真实运行未观察到蝴蝶停栖：'+repr(snapshots))
                    return
                self.report['cpuBehavior']['butterflyPerch']={
                    'simulated':int(butterfly[1]),'perched':int(butterfly[2]),'reservedSlots':int(butterfly[3]),
                    'renderingActive':bool(butterfly[4]),'source':'production CPU snapshot'}
                self.service.stop_all_ambient_fauna();self.phase='clear_automatic';self.mark=now;self.persist()
            elif self.phase=='clear_automatic' and elapsed>1:
                if self.active(): raise RuntimeError('自动场景StopAll仍有活跃群落')
                # 两个真实组件身份测试；脚本自行回收瞬态Actor，不触碰正式资产。
                runpy.run_path(str(ROOT/'Tests/VFX/BirdFlock/RunBirdFlockOwnershipReview.py'))
                runpy.run_path(str(ROOT/'Tests/VFX/BirdFlock/RunFaunaOwnershipReview.py'))
                self.report['ownershipScripts']='passed'
                self.actor=unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(
                    self.game,unreal.Actor,unreal.Transform(location=unreal.Vector(0,0,450)),
                    unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN)
                assert self.actor
                self.component=self.actor.add_component_by_class(
                    unreal.DivineBeastsFaunaSceneComponent,False,unreal.Transform(),False)
                self.component.set_editor_properties({'habitat_id':'DBA.Review.Fauna.RuntimeProbe','count':0,'seed':137})
                unreal.GameplayStatics.finish_spawning_actor(self.actor,unreal.Transform(location=unreal.Vector(0,0,450)))
                self.begin_case()
            elif self.phase=='settle' and elapsed>1.5:
                species,count=self.cases[self.index];active=self.active()
                if count==0:
                    if active: raise RuntimeError('数量0残留活跃Niagara')
                    self.report['gpuCases'].append({'species':species,'requested':0,'gpuObserved':0,'pass':True})
                    self.persist();self.begin_case();return
                selected=[c for c in active if c.get_asset().get_name()=='NS_DBA_'+species]
                if len(active)!=1 or len(selected)!=1:
                    if elapsed<20: return
                    raise RuntimeError('正式链路未创建唯一目标物种：'+repr([c.get_asset().get_name() for c in active]))
                state=self.component.query_fauna_state()
                if not self.accepted(state) or int(state[1])!=count or not bool(state[4]):
                    if elapsed<20: return
                    raise RuntimeError('生产CPU运行态不符：'+repr(state))
                self.capture_gpu(selected[0]);self.phase='gpu';self.mark=now
            elif self.phase=='gpu' and elapsed>0.5:
                species,count=self.cases[self.index]
                positions,orientations,motion=self.read_gpu()
                finite=all(math.isfinite(v) for p in positions for v in (p.x,p.y,p.z))
                normalized=len(orientations)==count and all(abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<0.01 for q in orientations)
                motion_valid=len(motion)==count and all(0<=m.x<1.001 and -.001<=m.y<=1.001 for m in motion)
                item={'species':species,'requested':count,'gpuObserved':len(positions),'finite':finite,
                      'normalizedOrientations':normalized,'validMotionParameters':motion_valid,
                      'pass':len(positions)==count and finite and normalized and motion_valid}
                self.report['gpuCases'].append(item);self.persist()
                if not item['pass']: raise RuntimeError('真实GPU缓存合同失败：'+repr(item))
                self.component.set_editor_property('count',513)
                if self.accepted(self.component.apply_fauna()): raise RuntimeError('513非法候选未被拒绝')
                if self.service.get_requested_fauna_count('DBA.Review.Fauna.RuntimeProbe')!=count:
                    raise RuntimeError('非法候选破坏旧有效请求')
                self.component.set_editor_property('count',count)
                if species=='FishSilver' and count==128:
                    self.fish_before=[(p.x,p.y,p.z) for p in positions]
                    self.phase='fish_move';self.mark=now
                else: self.begin_case()
            elif self.phase=='fish_move' and elapsed>1:
                active=self.active()
                if len(active)!=1: raise RuntimeError('鱼群移动观测期间实例丢失')
                self.capture_gpu(active[0]);self.phase='fish_read';self.mark=now
            elif self.phase=='fish_read' and elapsed>0.5:
                positions,_,_=self.read_gpu()
                moved=len(positions)==128 and any(sum((a-b)**2 for a,b in zip(old,(p.x,p.y,p.z)))>1 for old,p in zip(self.fish_before,positions))
                self.report['cpuBehavior']['fishGpuMotion']={'gpuCount':len(positions),'moved':moved}
                if not moved: raise RuntimeError('鱼群GPU位置没有持续推进')
                self.begin_case()
            elif self.phase=='final_clear' and elapsed>1:
                if self.active(): raise RuntimeError('最终StopAll未回收群落')
                self.actor.destroy_actor();self.actor=None;self.component=None
                self.report['stopAllActiveNiagara']=0
                self.levels.editor_request_end_play();self.phase='end';self.mark=now
            elif self.phase=='end' and elapsed>1:
                if self.editor.get_game_world() is not None: return
                self.report['worldCleanup']=True;self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc());self.finish('failed')
    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist();print('FAUNA_INTEGRATION_REVIEW',status)
        for error in self.report['errors']: unreal.log_error(error)
        if self.editor.get_game_world(): self.levels.editor_request_end_play()
        # 保留本任务编辑器供root读取画面/证据；不保存正在运行的地图。
unreal._fauna_integration_review=FaunaIntegrationReview()
