#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""当前UE编辑器的独立临时鸟群GPU检验会话，不切关卡、不保存当前地图、不更改永久资产。
临时Actor全部位于远离当前场景的空域，关闭Niagara距离预算仅用于测试；局部点光源不影响原场景。
每帧通过Slate回调允许GPU真实调度，读取SimCache检查各物种0/1/32/128/512数量和个体分布。
自动清理只删除本会话创建的transient对象；截图是UE SceneCapture真实光照结果，不包含UI。
"""
from pathlib import Path
import json
import traceback
import time
import unreal

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/BirdFlockP4'
BASE='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_'

class BirdGPUCapture:
    def __init__(self):
        self.world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.owned=[];self.components=[];self.frame=0;self.handle=None;self.cache=None
        self.cases=[];self.status={'status':'running','frames':[],'cases':self.cases,'humanAcceptance':'pending'}
        self.origin=unreal.Vector(100000,100000,100000)
        self.start_time=time.monotonic()
        OUT.mkdir(parents=True,exist_ok=True)
        self.selected=list(self.actors.get_selected_level_actors())
        try:
            self.prepare()
            self.actors.set_selected_level_actors(self.selected)
            self.handle=unreal.register_slate_post_tick_callback(self.tick)
            self.persist()
        except Exception:
            self.status['status']='failed';self.status['error']=traceback.format_exc();self.cleanup()
            self.actors.set_selected_level_actors(self.selected)
            raise

    def spawn(self,kind,label,offset,rotation=(0,0,0)):
        p=self.origin+unreal.Vector(*offset)
        a=self.actors.spawn_actor_from_class(kind,p,unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]),transient=True)
        if a is None: raise RuntimeError('临时Actor创建失败:'+label)
        a.set_actor_label('BirdTransient_'+label)
        self.owned.append(a)
        return a

    def prepare(self):
        for s,x,count in (('Seagull',-4200,32),('Swallow',0,72),('Eagle',4200,5)):
            a=self.spawn(unreal.NiagaraActor,s,(x,0,0))
            c=a.get_component_by_class(unreal.NiagaraComponent)
            c.set_editor_property('auto_activate',False)
            c.set_editor_property('allow_scalability',False)
            # 隔离主世界太阳的高照度，测试空域只接收本会话通道1的局部光。
            c.set_lighting_channels(False,True,False)
            c.set_force_solo(True)
            system=unreal.EditorAssetLibrary.load_asset(BASE+s)
            assert isinstance(system,unreal.NiagaraSystem)
            c.set_asset(system);c.set_variable_int('User.BirdCount',count);c.activate(True)
            self.components.append((s,c,a))
        # 两盏局部光只照射测试空域，不改变当前世界的太阳、天光、后处理或资产。
        for name,pos,intensity in (('KeyLight',(-5000,-5000,6500),200000),('FillLight',(5000,2000,5500),80000)):
            light=self.spawn(unreal.PointLight,name,pos)
            lc=light.get_component_by_class(unreal.PointLightComponent)
            channel=unreal.LightingChannels(channel0=False,channel1=True,channel2=False)
            lc.set_editor_property('lighting_channels',channel)
            lc.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
            lc.set_intensity(float(intensity));lc.set_attenuation_radius(23000.0)
            lc.set_editor_property('cast_shadows',False)
        self.capture_actor=self.spawn(unreal.SceneCapture2D,'Capture',(0,-12500,4300),(-15,90,0))
        self.capture=self.capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.rt=unreal.RenderingLibrary.create_render_target2d(self.world,1800,1000,unreal.TextureRenderTargetFormat.RTF_RGBA8,unreal.LinearColor(.03,.06,.12,1))
        self.capture.set_editor_property('texture_target',self.rt)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('capture_every_frame',False)
        self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('fov_angle',70.0)
        # 固定曝光并排除主世界的雾/大气，避免背景天空自动曝光把整幅审核图压成纯白。
        pp=unreal.PostProcessSettings()
        pp.set_editor_property('override_auto_exposure_method',True)
        pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_bias',True)
        pp.set_editor_property('auto_exposure_bias',-4.0)
        pp.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
        pp.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
        self.capture.set_editor_property('post_process_settings',pp)
        self.capture.set_editor_property('post_process_blend_weight',1.0)
        flags=[]
        for flag in ('Fog','Atmosphere','VolumetricFog','Bloom','MotionBlur'):
            entry=unreal.EngineShowFlagsSetting();entry.set_editor_property('show_flag_name',flag);entry.set_editor_property('enabled',False);flags.append(entry)
        self.capture.set_editor_property('show_flag_settings',flags)
        self.status['camera']={'location':str(self.capture.get_world_location()),'rotation':str(self.capture.get_world_rotation())}
        self.capture.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        # 使用运行时白名单API，不通过EditInstanceOnly属性修改临时组件模板标记。
        for _,_,actor in self.components:
            self.capture.show_only_actor_components(actor)
        self.tests=[(s,count) for s,_,_ in self.components for count in (0,1,32,128,512)]
        self.test_index=-1;self.test_frame=0;self.capture_cache=None;self.close_current=-1

    def persist(self):
        (OUT/'GPUSceneRuntime.json').write_text(json.dumps(self.status,ensure_ascii=False,indent=2),encoding='utf-8')

    def save_image(self,name):
        unreal.RenderingLibrary.export_render_target(self.world,self.rt,str(OUT),name)
        path=OUT/name
        if not path.is_file() or path.stat().st_size<1000: raise RuntimeError('实际渲染输出未保存:'+str(path))
        self.status['frames'].append({'path':name,'bytes':path.stat().st_size,'tick':self.frame})
        print('BIRD_GPU_SCENE_FRAME',name,path.stat().st_size)

    def tick(self,dt):
        try:
            self.frame+=1
            for _,c,_ in self.components:
                c.advance_simulation(1,1.0/30.0)
            self.capture.capture_scene()
            if self.frame in (75,145):
                self.save_image('BirdFlock_LitFrame_%03d.png'%self.frame)
            if self.frame<160: return
            # 三物种近景真实GPU截图，保留羽色/振翅差异，不把远景白点作为美术验收。
            if self.frame<340:
                index=(self.frame-160)//60
                species,_,actor=self.components[index]
                if index!=self.close_current:
                    self.close_current=index
                    x=(-4200,0,4200)[index]
                    self.capture_actor.set_actor_location(self.origin+unreal.Vector(x,-1100,950),False,False)
                    self.capture_actor.set_actor_rotation(unreal.Rotator(pitch=-22,yaw=90,roll=0),False)
                    self.capture.set_editor_property('fov_angle',48.0)
                    for s,c,_ in self.components:
                        c.set_variable_int('User.BirdCount',1 if s==species else 0)
                        c.set_variable_float('User.FlockRadius',250.0)
                        c.set_variable_float('User.FlockHeight',500.0)
                        c.set_variable_float('User.FlightSpeed',50.0)
                        c.reinitialize_system()
                if (self.frame-160)%60==50:
                    self.save_image('BirdFlock_'+species+'_GPU_Closeup.png')
                return
            if self.test_index<0:
                self.next_case()
            self.test_frame+=1
            species,count=self.tests[self.test_index]
            c=next(c for s,c,_ in self.components if s==species)
            if self.test_frame==35:
                cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
                params=unreal.NiagaraSimCacheCreateParameters()
                self.capture_cache=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,params,c,False,1.0/30.0)
                self.cache=cache
            if self.test_frame==45:
                positions=[]
                if self.capture_cache is not None and self.cache.get_num_frames()>0:
                    positions=list(self.cache.read_position_attribute('Position','BirdFlock',False,0))
                observed=len(positions)
                unique=len({(round(p.x,1),round(p.y,1),round(p.z,1)) for p in positions})
                import math
                finite=all(math.isfinite(v) for p in positions for v in (p.x,p.y,p.z))
                within_bounds=all(abs(p.x)<24000 and abs(p.y)<16000 and -1000<p.z<12000 for p in positions)
                case={'species':species,'requested':count,'gpuObserved':observed,'uniquePositions':unique,'finite':finite,'withinLocalBounds':within_bounds,'pass':observed==count and finite and within_bounds and (count<=1 or unique>1)}
                self.cases.append(case)
                self.persist()
                print('BIRD_GPU_COUNT_CASE',json.dumps(case))
                if self.test_index+1>=len(self.tests):
                    self.status['status']='passed' if all(x['pass'] for x in self.cases) else 'failed'
                    self.status['elapsedSeconds']=time.monotonic()-self.start_time
                    self.cleanup()
                else:
                    self.next_case()
        except Exception:
            self.status['status']='failed';self.status['error']=traceback.format_exc()
            unreal.log_error(self.status['error']);self.cleanup()

    def next_case(self):
        self.test_index+=1;self.test_frame=0;self.cache=None;self.capture_cache=None
        species,count=self.tests[self.test_index]
        for s,c,_ in self.components:
            c.set_variable_int('User.BirdCount',count if s==species else 0)
            c.reinitialize_system()

    def cleanup(self):
        if self.handle is not None:
            unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        # 不删除任何非本会话创建的Actor，不保存或切换用户当前地图。
        for a in reversed(self.owned):
            try: self.actors.destroy_actor(a)
            except Exception as exc: self.status.setdefault('cleanupWarnings',[]).append(str(exc))
        self.owned=[]
        self.persist()
        print('BIRD_GPU_SCENE_FINISHED',self.status['status'])

SESSION=BirdGPUCapture()
