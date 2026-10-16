# -*- coding: utf-8 -*-
"""SSLM花瓣原资源的短期PIE取证；只生成瞬态组件与相机，不保存地图、不操作账号。

由Monolith在本轮拥有的编辑器执行。原生游戏帧推进一次激活的Niagara，六秒治疗时长；
最终LDR经RenderTarget导出，不使用存在Gamma问题的Widget截图通道。失败也释放自有对象。
"""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OWNER=json.loads((ROOT/'Saved/Validation/PetalBloomIntegration20261015/Editor.json').read_text(encoding='utf-8-sig'))
assert OWNER['PID']==os.getpid()


class PetalCapture:
    """有限生命周期的开发采样；帧回调仅等待渲染，不读取或更新业务UI状态。"""
    def __init__(self):
        self.directory=Path(OWNER['Directory'])/'PetalFrame';self.directory.mkdir(exist_ok=True)
        self.world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        self.actors=[];self.api=unreal.get_default_object(unreal.GameplayStatics)
        self.started=time.monotonic();self.phase='warm'
        self.report=dict(status='running',renderer='UE5.8 PIE FinalColorLDR',manualAccepted=False)
        origin=unreal.Vector(0,0,5000)
        self.component=unreal.NiagaraFunctionLibrary.spawn_system_at_location(self.world,unreal.load_asset('/DBAPresentationPack_Core/VFX/Library/07_Status/NS_Nature_PetalBloom_Aura'),origin,auto_destroy=False,auto_activate=False,pre_cull_check=False)
        self.component.set_editor_property('allow_scalability',False)
        self.component.set_editor_property('wait_for_compilation_on_activate',True)
        self.component.set_force_solo(True);self.component.set_variable_float('User.SpellDuration',6.0)
        self.component.activate(True)
        floor=self.spawn(unreal.StaticMeshActor,origin+unreal.Vector(0,0,-2))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        floor.set_actor_scale3d(unreal.Vector(16,16,1));floor.set_actor_enable_collision(False)
        camera=self.spawn(unreal.SceneCapture2D,origin+unreal.Vector(420,-420,336))
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),origin+unreal.Vector(0,0,115)),False)
        self.capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.capture.set_editor_property('capture_every_frame',False);self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('fov_angle',48)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        self.capture.set_editor_property('texture_target',self.target)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def spawn(self,cls,location):
        """通过公开原生反射生成本PIE世界拥有的Actor；结束时逐一销毁。"""
        transform=unreal.Transform(location=location)
        actor=self.api.call_method('BeginDeferredActorSpawnFromClass',args=(self.world,cls.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.api.call_method('FinishSpawningActor',args=(actor,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.actors.append(actor);return actor

    def tick(self,delta):
        """一次激活后等待真实模拟；GPU取证与导出分帧，异常统一收敛为失败。"""
        try:
            elapsed=time.monotonic()-self.started
            if elapsed>30:raise RuntimeError('花瓣PIE取证超时')
            if self.phase=='warm' and elapsed>3:
                self.component.set_component_tick_enabled(False)
                cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.component)
                valid=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),self.component,False,1/30)
                self.cache=cache;self.report['simulationCaptured']=bool(valid)
                self.report['emitters']={}
                for index in range(cache.get_num_emitters()):
                    name=str(cache.get_emitter_name(index));colors=list(cache.read_color_attribute('Color',name,0))
                    self.report['emitters'][name]=dict(particles=len(colors),maxAlpha=max([c.a for c in colors],default=0))
                self.capture.capture_scene();self.phase='export';self.mark=time.monotonic()
            elif self.phase=='export' and time.monotonic()-self.mark>.8:
                unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.directory),'PetalBloom.png')
                image=self.directory/'PetalBloom.png'
                assert image.is_file() and image.stat().st_size>3000
                assert self.report['simulationCaptured'] and len(self.report['emitters'])==9
                assert all(v['particles']>0 and v['maxAlpha']>0 for v in self.report['emitters'].values())
                self.report.update(status='passed',image=str(image),bytes=image.stat().st_size);self.finish()
        except Exception:
            self.report.update(status='failed',error=traceback.format_exc());self.finish()

    def finish(self):
        """先解绑回调，再销毁瞬态表现对象；不留下常驻采样或重复订阅。"""
        unreal.unregister_slate_post_tick_callback(self.handle)
        if unreal.SystemLibrary.is_valid(self.component):self.component.destroy_component(self.component)
        for actor in reversed(self.actors):
            if unreal.SystemLibrary.is_valid(actor):actor.call_method('K2_DestroyActor')
        (self.directory/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')


petal_capture=PetalCapture()
