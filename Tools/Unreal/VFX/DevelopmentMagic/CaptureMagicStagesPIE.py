# -*- coding: utf-8 -*-
"""独占Monolith编辑器的PIE真实场景取证：冰箭四阶段及受疗树叶。

不保存关卡，不操作玩家账号。仅在显式PID匹配时生成瞬态Niagara/相机/地板，
使用原生SceneCapture的最终LDR渲染目标导出PNG；退出/失败销毁全部自有实例。
这不是UI轮询或玩法计时，所有采样与帧等待仅限本次开发验证。
"""
import json
import os
from pathlib import Path
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OWNER=json.loads((ROOT/'Saved/Validation/IceArrowVisualFollowup/Latest.json').read_text(encoding='utf-8-sig'))
assert OWNER['PID']==os.getpid()


class MagicStageCapture:
    """PIE世界中只读GPU与真实场景像素；所有对象均由此短期采样所有者清理。"""
    def __init__(self):
        self.directory=Path(OWNER['Directory'])/'StageFrames';self.directory.mkdir(exist_ok=True)
        self.world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
        self.actors=[];self.components={};self.cameras={};self.targets={};self.caches={}
        self.api=unreal.get_default_object(unreal.GameplayStatics)
        self.report={'status':'running','renderer':'UE5.8 PIE SceneCapture FinalColorLDR','frames':{},'errors':[],'manualAccepted':False}
        self.started=time.monotonic();self.phase='warm';self.mark=self.started
        for index,stage in enumerate(['Cast','Flight','Impact','FrostResidue','HealingLeaves']):
            origin=unreal.Vector(index*1600,0,5000)
            asset=('/DBAPresentationPack_Core/VFX/HealingLeaves/Niagara/NS_DBA_HealingLeaves' if stage=='HealingLeaves' else '/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_'+stage)
            component=unreal.NiagaraFunctionLibrary.spawn_system_at_location(self.world,unreal.load_asset(asset),origin,auto_destroy=False,auto_activate=False,pre_cull_check=False)
            component.set_editor_property('allow_scalability',False);component.set_editor_property('wait_for_compilation_on_activate',True);component.set_force_solo(True);component.activate(True)
            self.components[stage]=component
            if stage!='Flight':
                floor=self.spawn(unreal.StaticMeshActor,origin+unreal.Vector(0,0,-2))
                floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
                floor.set_actor_scale3d(unreal.Vector(16,16,1));floor.set_actor_enable_collision(False)
            distance=620 if stage in ['Cast','Impact','FrostResidue'] else 420
            camera=self.spawn(unreal.SceneCapture2D,origin+unreal.Vector(distance,-distance,distance*.8))
            aim=origin+unreal.Vector(-40 if stage=='Flight' else 0,0,105 if stage in ['Cast','Impact','HealingLeaves'] else 0)
            camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),aim),False)
            capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
            capture.set_editor_property('capture_every_frame',False);capture.set_editor_property('capture_on_movement',False)
            capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
            capture.set_editor_property('fov_angle',48)
            target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
            capture.set_editor_property('texture_target',target);self.targets[stage]=target;self.cameras[stage]=capture
        self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist()

    def spawn(self,cls,location):
        """反射调用公开引擎延迟生成API；Python隐藏胶水不影响真实World归属。"""
        transform=unreal.Transform(location=location)
        actor=self.api.call_method('BeginDeferredActorSpawnFromClass',args=(self.world,cls.static_class(),transform,unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN,None,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.api.call_method('FinishSpawningActor',args=(actor,transform,unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        self.actors.append(actor);return actor

    def persist(self):
        (self.directory/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')

    def tick(self,delta):
        try:
            now=time.monotonic()
            if now-self.started>35:raise RuntimeError('PIE图形采样超时')
            if self.phase=='warm' and now-self.mark>.4:
                for component in self.components.values():
                    component.activate(True);component.advance_simulation(6,1/30);component.set_component_tick_enabled(False)
                self.phase='capture';self.mark=now
            elif self.phase=='capture' and now-self.mark>.5:
                for stage,component in self.components.items():
                    cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(component)
                    valid=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,unreal.NiagaraSimCacheCreateParameters(),component,False,1/30)
                    self.caches[stage]=cache;self.report['frames'][stage]={'simulationCaptured':bool(valid)}
                    self.cameras[stage].capture_scene()
                self.phase='export';self.mark=now
            elif self.phase=='export' and now-self.mark>.8:
                for stage,target in self.targets.items():
                    unreal.RenderingLibrary.export_render_target(self.world,target,str(self.directory),stage+'.png')
                    path=self.directory/(stage+'.png');assert path.is_file() and path.stat().st_size>3000,'没有生成真实画面：'+stage
                    row=self.report['frames'][stage];row['path']=str(path);row['bytes']=path.stat().st_size
                    cache=self.caches[stage];row['simCacheFrames']=cache.get_num_frames();row['emitters']={}
                    assert row['simulationCaptured'] and row['simCacheFrames']>0,'真实模拟未建立：'+stage
                    for index in range(cache.get_num_emitters()):
                        name=str(cache.get_emitter_name(index));colors=list(cache.read_color_attribute('Color',name,0))
                        row['emitters'][name]={'particles':len(colors),'maxAlpha':max([c.a for c in colors],default=0)}
                self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc());self.finish('failed')

    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        for component in self.components.values():
            if unreal.SystemLibrary.is_valid(component):component.destroy_component(component)
        for actor in reversed(self.actors):
            if unreal.SystemLibrary.is_valid(actor):actor.call_method('K2_DestroyActor')
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started;self.persist()
        print('MAGIC_PIE_STAGE_CAPTURE',status,str(self.directory))


unreal._magic_stage_capture=MagicStageCapture()
