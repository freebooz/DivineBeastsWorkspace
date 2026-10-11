#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P6稳定性验证：真实PIE的三群各512只持续运行10分钟，定时GPU读回与生命周期清理。

复用已通过的正式表现链路验证器，不修改C++运行时或跳过Scalability、Data租约。
只在本任务独立编辑器执行，不保存资源。计数与稳定性不等于目标设备帧耗指标；
同期服务器编译和资源烘焙有CPU干扰，不把本运行的帧率宣称为产品性能基线。
"""
from pathlib import Path
import importlib.util
import json
import math
import time
import unreal

ROOT=Path(__file__).resolve().parents[4]
base_path=ROOT/'Tools/Unreal/VFX/BirdFlock/RunBirdFlockIntegrationReview.py'
# 只加载实现类定义，不执行该文件底部的SESSION实例化，避免两个验证器同时控制PIE。
namespace={'__file__':str(base_path),'__name__':'BirdFlockIntegrationReviewBase'}
source=base_path.read_text(encoding='utf-8')
marker='SESSION=IntegrationReview()'
if source.count(marker)!=1:
    raise RuntimeError('正式链路测试实现入口发生变化，拒绝猜测启动')
exec(compile(source.replace(marker,''),str(base_path),'exec'),namespace)
BaseReview=namespace['IntegrationReview']
namespace['OUT']=ROOT/'Saved/Validation/BirdFlockP4P7/Soak'


class BirdSoakReview(BaseReview):
    def __init__(self):
        self.soak_started=None;self.sample_index=0;self.next_sample=0.0
        self.pending_sample=None;self.soak_cleaning=False
        self.soak_samples=[];self.soak_duration=600.0
        self.timeout_seconds=720.0
        super().__init__()
        self.report['stabilitySamples']=self.soak_samples
        self.report['plannedSoakSeconds']=self.soak_duration
        self.report['performanceScope']='Concurrent development workstation; stability only, not target-device frame budget'
        self.persist()

    def next_case(self):
        if self.soak_started is None:
            # 自动场景验证和停止已由父类完成；随后用正式服务建立3群各512的稳定性压力场景。
            for species in ('Seagull','Swallow','Eagle'):
                self.service.start_ambient_bird_flock('DBA.Review.Soak.'+species,species,512,unreal.Vector(0,0,450))
            self.soak_started=time.monotonic();self.next_sample=self.soak_started+3.0
            self.phase='soak';self.phase_start=self.soak_started
            self.persist()
        else:
            raise RuntimeError('稳定性测试不应重复创建鸟群')

    def tick(self,delta):
        if self.phase!='soak':
            # 初始化和最终清理也保留原始开始时间；本类显式12分钟总上限覆盖10分钟采样。
            return super().tick(delta)
        try:
            now=time.monotonic();elapsed=now-self.soak_started
            if elapsed>self.soak_duration+45:
                raise RuntimeError('10分钟GPU稳定性检查超过有界结束窗口')
            if self.pending_sample is not None:
                if now-self.phase_start<0.45:
                    return
                species,component=self.pending_sample
                positions=list(self.cache.read_position_attribute('Position','BirdFlock',False,0)) if self.cache.get_num_frames()>0 else []
                valid=len(positions)==512 and all(math.isfinite(v) for p in positions for v in (p.x,p.y,p.z))
                sample={'elapsedSeconds':round(elapsed,3),'species':species,'gpuCount':len(positions),
                        'activeSystems':len(self.active_birds()),'pass':valid}
                self.soak_samples.append(sample);self.persist()
                print('BIRD_SOAK_GPU_SAMPLE',json.dumps(sample))
                if not valid or sample['activeSystems']!=3:
                    raise RuntimeError('持续运行发生粒子数量漂移/重复实例/无效位置：'+repr(sample))
                self.cache=None;self.gpu_task=None;self.pending_sample=None
                self.sample_index+=1
                self.next_sample=now+9.5
                if elapsed>=self.soak_duration:
                    self.report['completedSoakSeconds']=elapsed
                    self.service.stop_all_ambient_bird_flocks()
                    self.phase='stop_all';self.phase_start=now;self.soak_cleaning=True
                return
            if now<self.next_sample:
                return
            species=('Seagull','Swallow','Eagle')[self.sample_index%3]
            found=[c for c in self.active_birds() if c.get_asset().get_name()=='NS_DBA_BirdFlock_'+species]
            if len(found)!=1:
                raise RuntimeError('稳定性检查物种实例不唯一：'+species)
            self.cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(found[0])
            self.gpu_task=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                self.cache,unreal.NiagaraSimCacheCreateParameters(),found[0],False,1.0/30.0)
            self.pending_sample=(species,found[0]);self.phase_start=now
        except Exception:
            import traceback
            self.report['errors'].append(traceback.format_exc());self.finish('failed')


SESSION=BirdSoakReview()
