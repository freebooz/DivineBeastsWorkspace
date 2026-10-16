# -*- coding: utf-8 -*-
"""仅在本轮拥有的SSLM编辑器中，对8个提示系统和地面冰箭进行真实PIE渲染/SimCache取证。

临时组件、相机和有限采样由一个所有者清理；不保存地图、不读账号、不改变UI业务。
移动类样例通过真实组件位移及原有User位置参数提供连续轨迹，不以静止零长Ribbon冒充可见。
"""
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
DIRECTORY=ROOT/'Saved/Validation/VFXInitialization20261016'
OWNER=json.loads((DIRECTORY/'Editor.json').read_text(encoding='utf-8-sig'))
assert OWNER['PID']==os.getpid() and '-VFXInitializationOwned' in unreal.SystemLibrary.get_command_line()


class RendererCapture:
    """逐系统采样，避免同场多效果互相遮盖；记录空场基线与每个受影响发射器的真实属性。"""
    def __init__(self):
        self.audit=json.loads((DIRECTORY/'InitializationInputs.json').read_text(encoding='utf8'))
        self.cases=[dict(system=v['system'],parameters=v['userParameters']) for v in self.audit['systems']]
        self.cases.append(dict(system='/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Cast',parameters=[]))
        self.frames=DIRECTORY/'Frames';self.frames.mkdir(exist_ok=True)
        self.world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        self.api=unreal.get_default_object(unreal.GameplayStatics);self.actors=[];self.component=None
        self.report=dict(status='running',renderer='UE5.8 PIE FinalColorLDR',cases=[],manualAccepted=False)
        self.started=time.monotonic();self.index=-1;self.phase='baseline';self.camera=self.spawn(unreal.SceneCapture2D,unreal.Vector(780,-780,620))
        self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(self.camera.get_actor_location(),unreal.Vector(0,0,70)),False)
        self.capture=self.camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.capture.set_editor_property('capture_every_frame',False);self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('primitive_render_mode',unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('fov_angle',50)
        settings=unreal.PostProcessSettings()
        settings.set_editor_property('override_auto_exposure_method',True)
        settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
        settings.set_editor_property('override_auto_exposure_bias',True);settings.set_editor_property('auto_exposure_bias',0.0)
        self.capture.set_editor_property('post_process_settings',settings)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,960,540,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        self.capture.set_editor_property('texture_target',self.target);self.capture.capture_scene();self.mark=time.monotonic()
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()

    def spawn(self,cls,location):
        """使用公开引擎API创建本PIE世界的瞬态Actor，不向正式关卡添加内容。"""
        transform=unreal.Transform(location=location)
        actor=self.api.call_method('BeginDeferredActorSpawnFromClass',args=(self.world,cls.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.api.call_method('FinishSpawningActor',args=(actor,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.actors.append(actor);return actor

    def next_case(self):
        """先销毁前一组件再激活下一系统；每个组件只激活一次，不重复推进生成计数。"""
        if self.component and unreal.SystemLibrary.is_valid(self.component):self.component.destroy_component(self.component)
        self.capture.clear_show_only_components();self.index+=1
        if self.index==len(self.cases):self.report['status']='captured';self.finish();return
        case=self.cases[self.index];self.row=dict(system=case['system'],frames=[])
        self.report['cases'].append(self.row)
        self.component=unreal.NiagaraFunctionLibrary.spawn_system_at_location(self.world,unreal.load_asset(case['system']),unreal.Vector(),auto_destroy=False,auto_activate=False,pre_cull_check=False)
        self.component.set_editor_property('allow_scalability',False)
        self.component.set_editor_property('wait_for_compilation_on_activate',True);self.component.set_force_solo(True)
        self.capture.show_only_component(self.component)
        self.moving=any(v in case['system'] for v in ['Projectile','Impact_Legacy'])
        self.parameter_names={p['name'] for p in case['parameters']}
        self.update_pose(0)
        self.component.activate(True);self.case_started=unreal.GameplayStatics.get_time_seconds(self.world)
        self.samples=[0.12,0.4,2.0 if ('PetalsAura' in case['system'] or 'IceArrow_Cast' in case['system']) else 0.9]
        self.sample_index=0;self.phase='sample';self.persist()

    def update_pose(self,age):
        """测试输入提供位置/速度；只使用源系统已声明的公开User参数，不注入未知资源或玩法状态。"""
        position=unreal.Vector(age*260,math.sin(age*4)*55,20) if self.moving else unreal.Vector()
        self.component.set_world_location(position,False,False)
        for name in ['User.SourcePosition','User.OwnerPosition','User.TargetPosition','User.DestinationPosition']:
            if name in self.parameter_names:
                value=position+unreal.Vector(400,0,30) if name in ['User.TargetPosition','User.DestinationPosition'] else position
                self.component.set_variable_vec3(name,value)
        if 'User.OwnerVelocity' in self.parameter_names:self.component.set_variable_vec3('User.OwnerVelocity',unreal.Vector(260,220*math.cos(age*4),0))

    def snapshot(self,age):
        """原生缓存记录GPU回读/CPU数据；有效粒子按属性与Alpha同一索引匹配，禁止只凭编译认定可见。"""
        cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.component)
        valid=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),self.component,False,1/30)
        frame=dict(ageSeconds=age,simCacheValid=bool(valid),emitters={})
        if valid:
            for i in range(cache.get_num_emitters()):
                name=str(cache.get_emitter_name(i));colors=list(cache.read_color_attribute('Color',name,0))
                expected=next((e for e in self.audit['emitters'] if e['system']==self.row['system'] and e['emitter']==name),None)
                data=dict(particles=len(colors),maxAlpha=max([c.a for c in colors],default=0))
                if expected:
                    attribute=expected['attribute'];values=list(cache.read_vector2_attribute(attribute,name,0)) if attribute=='SpriteSize' else list(cache.read_float_attribute(attribute,name,0))
                    positive=[]
                    for j,value in enumerate(values):
                        dimensions=[value.x,value.y] if attribute=='SpriteSize' else [value]
                        if j<len(colors) and colors[j].a>0.001 and all(math.isfinite(v) and v>0 for v in dimensions):positive.append(j)
                    data.update(attribute=attribute,dimensionCount=len(values),positiveVisibleDimensions=len(positive))
                    if attribute=='RibbonWidth':
                        points=list(cache.read_position_attribute('Position',name,0));usable=[points[j] for j in positive if j<len(points)]
                        data['ribbonSpanCm']=max([(v-usable[0]).length() for v in usable],default=0) if usable else 0
                if 'IceArrow_Cast' in self.row['system']:
                    positions=list(cache.read_position_attribute('Position',name,0));rotations=list(cache.read_quat_attribute('MeshOrientation',name,0))
                    data['groundZ']=[v.z for v in positions];data['verticalTilt']=[abs(q.x)+abs(q.y) for q in rotations]
                frame['emitters'][name]=data
        self.cache=cache;self.row['frames'].append(frame);self.capture.capture_scene();self.mark=time.monotonic();self.phase='export'

    def tick(self,delta):
        """有限采样回调，超过90秒失败；所有退出路径都清理自有委托/组件。"""
        try:
            if time.monotonic()-self.started>90:raise RuntimeError('VFX有限采样超时')
            if self.phase=='baseline' and time.monotonic()-self.mark>.2:
                unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.frames),'Baseline.png');self.next_case()
            elif self.phase=='sample':
                age=unreal.GameplayStatics.get_time_seconds(self.world)-self.case_started;self.update_pose(age)
                if age>=self.samples[self.sample_index]:self.snapshot(age)
            elif self.phase=='export' and time.monotonic()-self.mark>.12:
                filename=f'{self.index:02}_{self.sample_index}_{self.row["system"].rsplit("/",1)[-1]}.png'
                unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.frames),filename)
                self.row['frames'][-1]['image']=str(self.frames/filename);self.sample_index+=1;self.persist()
                if self.sample_index==len(self.samples):self.next_case()
                else:self.phase='sample'
        except Exception:
            self.report.update(status='failed',error=traceback.format_exc());self.finish()

    def persist(self):
        (DIRECTORY/'Capture.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')

    def finish(self):
        """先解绑再销毁；不遗留正式World对象、轮询或业务UI订阅。"""
        unreal.unregister_slate_post_tick_callback(self.handle)
        if self.component and unreal.SystemLibrary.is_valid(self.component):self.component.destroy_component(self.component)
        for actor in reversed(self.actors):
            if unreal.SystemLibrary.is_valid(actor):actor.call_method('K2_DestroyActor')
        self.persist()


unreal._renderer_initialization_capture=RendererCapture()
