#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5/P6真实PIE链路验证：场景蓝图自动启动、三种物种切换、数量GPU读回、取消清理。

只在-BirdOwnedIntegrationReview标识的独立编辑器打开自有审核地图后执行，
通过-ExecCmds=py启动异步Slate检查。不会修改/保存地图或正式资源，不操作其他进程。
成功必须观察正式VFX执行器创建的Niagara组件与真实GPU计数，不能仅依赖请求返回True。
"""
from pathlib import Path
import json
import math
import time
import traceback
import unreal

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/BirdFlockP4P7/Integration'
MAP='/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockIntegrationReview'
BASE='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_'


class IntegrationReview:
    def __init__(self):
        if '-BirdOwnedIntegrationReview' not in unreal.SystemLibrary.get_command_line():
            raise RuntimeError('只允许独立的鸟群PIE验证进程')
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        world=self.editor.get_editor_world()
        if not world or world.get_path_name().split('.')[0]!=MAP:
            raise RuntimeError('不是鸟群正式链路审核地图')
        self.started=time.monotonic();self.phase_start=self.started
        self.phase='boot';self.game=None;self.service=None;self.handle=None
        self.cases=[(s,n) for s in ('Seagull','Swallow','Eagle') for n in (0,1,32,128,512)]
        self.index=-1;self.cache=None;self.gpu_task=None
        self.report={'status':'running','automaticScene':{},'cases':[],'errors':[],
                     'path':'SceneBlueprint->ProjectPresentation->PlatformPresentation->DataLease->VFX->Niagara',
                     'humanAcceptance':'pending'}
        OUT.mkdir(parents=True,exist_ok=True)
        self.levels.editor_set_viewport_realtime(True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        print('BIRD_REAL_INTEGRATION_STARTED')

    def persist(self):
        (OUT/'IntegrationReview.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')

    def active_birds(self):
        found=[]
        if self.game is None:
            return found
        for actor in unreal.GameplayStatics.get_all_actors_of_class(self.game,unreal.Actor):
            for c in actor.get_components_by_class(unreal.NiagaraComponent):
                system=c.get_asset()
                if system and system.get_path_name().startswith(BASE) and c.is_active():
                    found.append(c)
        return found

    def next_case(self):
        self.index+=1
        self.cache=None;self.gpu_task=None
        if self.index>=len(self.cases):
            self.service.stop_all_ambient_bird_flocks()
            self.phase='stop_all';self.phase_start=time.monotonic()
            return
        species,count=self.cases[self.index]
        result=self.service.start_ambient_bird_flock('DBA.Review.RuntimeProbe',species,count,unreal.Vector(0,0,450))
        self.last_result=repr(result)
        self.phase='settle';self.phase_start=time.monotonic()

    def tick(self,delta):
        try:
            now=time.monotonic();elapsed=now-self.phase_start
            if now-self.started>getattr(self,'timeout_seconds',240.0):
                raise RuntimeError('PIE正式链路验证超时')
            if self.phase=='boot' and elapsed>2:
                self.levels.editor_request_begin_play()
                self.phase='wait_world';self.phase_start=now
            elif self.phase=='wait_world':
                self.game=self.editor.get_game_world()
                if self.game is None:
                    if elapsed>25: raise RuntimeError('无法建立真实PIE游戏世界')
                    return
                # 此UE5.8的Python未暴露LP查询函数，PC.Player也不可读取。
                # 本进程只有一个受控PIE世界，枚举真实LocalPlayer拥有的唯一服务；
                # 多本地玩家/多服务直接失败，不猜测选择，不创建测试替身。
                services=[obj for obj in unreal.ObjectIterator(unreal.DivineBeastsPresentationClientSubsystem)
                          if isinstance(obj.get_outer(),unreal.LocalPlayer)]
                if len(services)>1:
                    raise RuntimeError('独立审核进程出现多个LocalPlayer服务，拒绝猜测')
                self.service=services[0] if len(services)==1 else None
                if self.service is None:
                    if elapsed>25: raise RuntimeError('游戏LocalPlayer正式表现子系统缺失')
                    return
                self.phase='automatic';self.phase_start=now
            elif self.phase=='automatic':
                components=self.active_birds()
                seen={c.get_asset().get_name().removeprefix('NS_DBA_BirdFlock_') for c in components}
                if seen!={'Seagull','Swallow','Eagle'} or len(components)!=3:
                    if elapsed>20:
                        raise RuntimeError('场景蓝图没有自动经正式链路生成三物种：'+repr([c.get_path_name() for c in components]))
                    return
                self.report['automaticScene']={'pass':True,'world':self.game.get_path_name(),
                    'niagaraSystems':[c.get_asset().get_path_name() for c in components],
                    'requested':[self.service.get_requested_bird_count('DBA.Review.Birds.'+s) for s in ('Seagull','Swallow','Eagle')]}
                self.service.stop_all_ambient_bird_flocks()
                self.persist();self.phase='automatic_clear';self.phase_start=now
            elif self.phase=='automatic_clear' and elapsed>1:
                if self.active_birds(): raise RuntimeError('自动场景鸟群取消后仍有活跃Niagara')
                self.next_case()
            elif self.phase=='settle' and elapsed>1.5:
                species,count=self.cases[self.index]
                components=self.active_birds()
                if count==0:
                    if components: raise RuntimeError('数量0未取消实际Niagara实例')
                    self.report['cases'].append({'species':species,'requested':0,'gpuObserved':0,'pass':True})
                    self.persist();self.next_case();return
                selected=[c for c in components if c.get_asset().get_name()=='NS_DBA_BirdFlock_'+species]
                if len(components)!=1 or len(selected)!=1:
                    if elapsed<15: return
                    raise RuntimeError('正式链路未生成唯一的期望物种Niagara：'+repr([c.get_asset().get_name() for c in components])+' submit='+self.last_result)
                self.cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(selected[0])
                self.gpu_task=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                    self.cache,unreal.NiagaraSimCacheCreateParameters(),selected[0],False,1.0/30.0)
                self.phase='read_gpu';self.phase_start=now
            elif self.phase=='read_gpu' and elapsed>.4:
                species,count=self.cases[self.index]
                positions=list(self.cache.read_position_attribute('Position','BirdFlock',False,0)) if self.cache.get_num_frames()>0 else []
                finite=all(math.isfinite(v) for p in positions for v in (p.x,p.y,p.z))
                item={'species':species,'requested':count,'gpuObserved':len(positions),
                      'finite':finite,'pass':len(positions)==count and finite}
                self.report['cases'].append(item);self.persist()
                print('BIRD_PIPELINE_GPU_CASE',json.dumps(item))
                if not item['pass']: raise RuntimeError('正式表现链路GPU数量不符：'+repr(item))
                # 非法输入必须不改变已经成功运行的请求与可视物种。
                bad_count=self.service.start_ambient_bird_flock('DBA.Review.RuntimeProbe',species,513,unreal.Vector(0,0,450))
                bad_species=self.service.start_ambient_bird_flock('DBA.Review.RuntimeProbe','UnknownBird',1,unreal.Vector(0,0,450))
                if self.service.get_requested_bird_count('DBA.Review.RuntimeProbe')!=count:
                    raise RuntimeError('非法数量/物种污染既有合法请求')
                item['invalidRequests']={'count513':repr(bad_count),'unknownSpecies':repr(bad_species)}
                self.next_case()
            elif self.phase=='stop_all' and elapsed>1:
                active=self.active_birds()
                self.report['stopAllActiveNiagara']=len(active)
                if active: raise RuntimeError('停止全部后仍有活跃鸟群')
                self.cache=None;self.gpu_task=None;self.service=None;self.game=None
                self.levels.editor_request_end_play();self.phase='end_pie';self.phase_start=now
            elif self.phase=='end_pie' and elapsed>1:
                if self.editor.get_game_world() is not None:
                    if elapsed>20: raise RuntimeError('结束PIE后游戏世界未清理')
                    return
                self.report['worldCleanup']=True
                self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.finish('failed')

    def finish(self,status):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist();print('BIRD_REAL_INTEGRATION_FINISHED',status)
        for error in self.report['errors']: unreal.log_error(error)
        if self.editor.get_game_world() is not None: self.levels.editor_request_end_play()
        unreal.SystemLibrary.quit_editor()


SESSION=IntegrationReview()
