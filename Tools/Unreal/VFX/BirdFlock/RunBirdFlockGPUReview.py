#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群P6独立真实GPU审核：编辑器视口截图、三物种15组GPU读回、位置运动校验。

只在明确带BIRD_OWNED_GPU_REVIEW=1的独立UE进程执行，必须已经打开本任务审核地图。
通过 -ExecCmds="py ..." 启动，让Slate/GPU继续真实调度；完成后正常退出自己的编辑器。
不修改/保存任何正式资源，不连接/操作其他任务的Monolith或编辑器窗口。
截图直接来自UE高分辨率视口，不复用历史SceneCapture输出，不生成示意假图。
"""
from pathlib import Path
import hashlib
import json
import math
import os
import time
import traceback
import unreal

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/BirdFlockP4P7/GPUReview'
MAP='/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview'
SPECIES=('Seagull','Swallow','Eagle')


class BirdGPUReview:
    def __init__(self):
        if os.getenv('BIRD_OWNED_GPU_REVIEW')!='1' and '-BirdOwnedGPUReview' not in unreal.SystemLibrary.get_command_line():
            raise RuntimeError('必须在本轮独立拥有的审核进程执行')
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.world=self.editor.get_editor_world()
        if not self.world or self.world.get_path_name().split('.')[0]!=MAP:
            raise RuntimeError('只能操作鸟群自有GPU审核地图')
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.actor_api=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.components={}
        self.frame=0
        self.started=time.monotonic()
        self.last_simulated=self.started
        self.phase='warmup'
        self.phase_start=self.started
        self.index=-1
        self.tasks=[]
        self.cache=None
        self.capture_task=None
        self.pending_image=None
        self.handle=None
        self.report={'status':'running','frames':[],'cases':[], 'errors':[],
                     'source':'UE5.8 real viewport and Niagara GPU SimCache',
                     'humanAcceptance':'pending'}
        OUT.mkdir(parents=True,exist_ok=True)
        for species in SPECIES:
            matches=[a for a in self.actor_api.get_all_level_actors()
                     if a.get_actor_label()=='BirdReview_'+species]
            if len(matches)!=1:
                raise RuntimeError('审核地图物种Actor不唯一：'+species)
            component=matches[0].get_component_by_class(unreal.NiagaraComponent)
            if component is None or component.get_asset() is None:
                raise RuntimeError('审核地图没有真实Niagara组件：'+species)
            # 仅审核夹具取消距离剔除：GPU数量边界读回不被测试相机位置污染。
            component.set_editor_property('allow_scalability',False)
            component.set_force_solo(True)
            component.activate(True)
            # 编辑器非PIE的粒子不是游戏World自动模拟；本审核采用唯一的手动时钟。
            component.set_component_tick_enabled(False)
            self.components[species]=(matches[0],component)
        cameras=[a for a in self.actor_api.get_all_level_actors() if a.get_actor_label()=='BirdReview_Camera']
        if len(cameras)!=1:
            raise RuntimeError('审核相机不唯一')
        self.camera=cameras[0]
        self.camera_component=self.camera.get_component_by_class(unreal.CameraComponent)
        # 审核背景使用引擎中性材质，不让默认网格/噪声纹理干扰羽色和翼面瑕疵观察。
        neutral=unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/BasicShapeMaterial')
        if neutral:
            for actor in self.actor_api.get_all_level_actors():
                if isinstance(actor,unreal.StaticMeshActor):
                    actor.get_component_by_class(unreal.StaticMeshComponent).set_material(0,neutral)
        pp=self.camera_component.get_editor_property('post_process_settings')
        pp.set_editor_property('override_film_grain_intensity',True)
        pp.set_editor_property('film_grain_intensity',0.0)
        pp.set_editor_property('override_motion_blur_amount',True)
        pp.set_editor_property('motion_blur_amount',0.0)
        self.camera_component.set_editor_property('post_process_settings',pp)
        self.levels.editor_set_viewport_realtime(True)
        self.levels.editor_set_game_view(True)
        unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
        self.move_camera(unreal.Vector(0,-11500,5500),unreal.Vector(0,0,1200),65.0)
        self.cases=[(s,n) for s in SPECIES for n in (0,1,32,128,512)]
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        print('BIRD_OWNED_GPU_REVIEW_STARTED',MAP)

    def persist(self):
        (OUT/'GPUReview.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')

    def move_camera(self,location,target,fov):
        rotation=unreal.MathLibrary.find_look_at_rotation(location,target)
        self.camera.set_actor_location(location,False,False)
        self.camera.set_actor_rotation(rotation,False)
        self.camera_component.set_field_of_view(fov)
        self.levels.pilot_level_actor(self.camera)
        self.levels.editor_invalidate_viewports()

    def request_frame(self,name):
        path=OUT/name
        if path.exists():
            # 本任务输出不允许从旧图证明通过；每轮原始图片名字包含时间戳。
            path=OUT/(path.stem+'_'+str(int(time.time()))+'.png')
        task=unreal.AutomationLibrary.take_high_res_screenshot(
            1920,1080,str(path),camera=self.camera,mask_enabled=False,capture_hdr=False,
            delay=0.15,force_game_view=True)
        if task is None:
            raise RuntimeError('实际UE截图任务没有建立')
        self.tasks.append(task)
        self.pending_image=(path,time.monotonic())

    def frame_ready(self):
        path,requested=self.pending_image
        # 高分截图是异步渲染，至少等待两个真实时钟秒再读文件；绝不在同一回调立即复制旧缓冲。
        if time.monotonic()-requested<2.0:
            return False
        if not path.is_file():
            if time.monotonic()-requested>35:
                raise RuntimeError('实际UE视口未保存截图：'+str(path))
            return False
        data=path.read_bytes()
        if len(data)<2000:
            raise RuntimeError('截图文件异常小')
        item={'path':str(path.relative_to(OUT)), 'bytes':len(data),
              'sha256':hashlib.sha256(data).hexdigest(), 'frame':self.frame}
        self.report['frames'].append(item)
        print('BIRD_REAL_VIEWPORT_FRAME',json.dumps(item))
        self.pending_image=None
        self.persist()
        return True

    def configure_closeup(self,species):
        for name,(actor,component) in self.components.items():
            component.set_variable_int('User.BirdCount',1 if name==species else 0)
            component.set_variable_float('User.FlockRadius',210.0)
            component.set_variable_float('User.FlockHeight',500.0)
            component.set_variable_float('User.FlightSpeed',100.0)
            component.reinitialize_system()
            component.set_component_tick_enabled(False)
        origin=self.components[species][0].get_actor_location()
        self.move_camera(origin+unreal.Vector(550,-650,820),origin+unreal.Vector(0,0,500),52.0)

    def next_case(self):
        self.index+=1
        if self.index>=len(self.cases):
            hashes={f['sha256'] for f in self.report['frames']}
            if len(hashes)<len(self.report['frames']):
                raise RuntimeError('不同帧/物种得到重复截图，不能通过视觉证据门禁')
            self.finish('passed')
            return
        species,count=self.cases[self.index]
        for name,(actor,component) in self.components.items():
            component.set_variable_int('User.BirdCount',count if name==species else 0)
            component.set_variable_float('User.FlockRadius',1700.0)
            component.set_variable_float('User.FlockHeight',850.0)
            component.set_variable_float('User.FlightSpeed',250.0)
            component.reinitialize_system()
            component.set_component_tick_enabled(False)
        self.phase='case_settle'
        self.phase_start=time.monotonic()
        self.cache=None
        self.capture_task=None

    def tick(self,delta):
        try:
            self.frame+=1
            now=time.monotonic()
            if now-self.started>420:
                raise RuntimeError('独立GPU审核超时')
            # ForceSolo+禁用组件Tick后，唯一时钟按实际时间最多30Hz推进GPU，
            # 避免编辑器默认不模拟以及自动/手动两路同时排队Burst的问题。
            if now-self.last_simulated>=1.0/30.0:
                dt=min(now-self.last_simulated,0.1)
                for _,component in self.components.values():
                    component.advance_simulation(1,dt)
                self.last_simulated=now
            elapsed=now-self.phase_start
            if self.phase=='warmup' and elapsed>6:
                self.request_frame('BirdFlock_Overview_A.png')
                self.phase='overview_a'
            elif self.phase=='overview_a' and self.frame_ready():
                self.phase='overview_wait';self.phase_start=now
            elif self.phase=='overview_wait' and elapsed>3:
                self.request_frame('BirdFlock_Overview_B.png');self.phase='overview_b'
            elif self.phase=='overview_b' and self.frame_ready():
                self.index=0;self.configure_closeup(SPECIES[self.index])
                self.phase='close_settle';self.phase_start=now
            elif self.phase=='close_settle' and elapsed>3:
                self.request_frame('BirdFlock_'+SPECIES[self.index]+'_Lit.png')
                self.phase='close_frame'
            elif self.phase=='close_frame' and self.frame_ready():
                self.index+=1
                if self.index<len(SPECIES):
                    self.configure_closeup(SPECIES[self.index]);self.phase='close_settle';self.phase_start=now
                else:
                    self.index=-1;self.next_case()
            elif self.phase=='case_settle' and elapsed>1.6:
                species,count=self.cases[self.index]
                component=self.components[species][1]
                self.cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(component)
                self.capture_task=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                    self.cache,unreal.NiagaraSimCacheCreateParameters(),component,False,1.0/30.0)
                self.phase='case_read';self.phase_start=now
            elif self.phase=='case_read' and elapsed>0.45:
                species,count=self.cases[self.index]
                positions=[]
                if self.cache and self.cache.get_num_frames()>0:
                    positions=list(self.cache.read_position_attribute('Position','BirdFlock',False,0))
                observed=len(positions)
                finite=all(math.isfinite(v) for p in positions for v in (p.x,p.y,p.z))
                unique=len({(round(p.x,2),round(p.y,2),round(p.z,2)) for p in positions})
                item={'species':species,'requested':count,'gpuObserved':observed,
                    'uniquePositions':unique,'finite':finite,
                    'pass':observed==count and finite and (count<=1 or unique>1)}
                self.report['cases'].append(item)
                print('BIRD_REAL_GPU_COUNT',json.dumps(item))
                self.persist()
                if not item['pass']:
                    raise RuntimeError('实际GPU数量读回失败：'+repr(item))
                self.next_case()
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.finish('failed')

    def finish(self,status):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status
        self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist()
        print('BIRD_REAL_GPU_REVIEW_FINISHED',status)
        for error in self.report['errors']:
            unreal.log_error(error)
        # 整个进程属于本工具，无保存正式场景；退出不影响其他编辑器或工作窗口。
        unreal.SystemLibrary.quit_editor()


SESSION=BirdGPUReview()
