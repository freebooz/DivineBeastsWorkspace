# -*- coding: utf-8 -*-
"""SSLM冰箭真实GPU颜色/位置探针，仅由Monolith在独占UE5.8编辑器运行。

临时NiagaraActor不保存到关卡；Slate回调只推进本探针组件和读取原生GPU SimCache，
结束/失败均清理自有Actor与回调。采样不是人工视觉、联网或Cook验收，不改生产播放器。
"""
import json
import hashlib
import math
import os
from pathlib import Path
import time
import traceback
import unreal


class IceArrowGPUProbe:
    """六个真实系统独立手动推进至0.1/0.2秒，GPU回读记录每个发射器的Color与局部位置。"""
    def __init__(self, stages, output_name, expected_editor_pid, capture_viewport=False):
        assert expected_editor_pid > 0 and os.getpid() == expected_editor_pid, '冰箭GPU探针编辑器所有权变化'
        self.root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
        assert (self.root / 'Game/DivineBeastsArena.uproject').is_file()
        assert output_name.isidentifier(), '证据文件名只接受稳定英文标识'
        self.path = self.root / 'Saved/Validation/WorldEntryFix20261015/IceArrowNativeGraphRepair' / (output_name + '.json')
        self.api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.components, self.actors, self.caches, self.tasks = {}, [], {}, []
        self.started = time.monotonic()
        self.mark, self.last = self.started, self.started
        self.phase, self.steps, self.sample = 'await_activation', 0, 0
        self.ready_stages = set()
        # 只在调用方显式要求时使用真实编辑器世界截图，避免FPreviewScene的GPU黑图被误作验收。
        self.capture_viewport = capture_viewport
        self.pending_frame = None
        self.report = {'status': 'running', 'source': 'UE5.8 real GPU NiagaraSimCache',
                       'stages': stages, 'samples': [], 'errors': [], 'manualVisualAccepted': False}
        for index, stage in enumerate(stages):
            actor = self.api.spawn_actor_from_class(unreal.NiagaraActor,
                unreal.Vector(index * 800, 0, 1200), transient=True)
            self.actors.append(actor)
            actor.set_actor_label('IceArrowGPUProbe_' + stage)
            component = actor.get_component_by_class(unreal.NiagaraComponent)
            component.set_auto_activate(False)
            component.set_asset(unreal.load_asset('/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_' + stage))
            component.set_editor_property('allow_scalability', False)
            # 冷载入必须等待真实GPU编译；仅VM编译UpToDate不能保证组件已具有SystemSimulation。
            component.set_editor_property('wait_for_compilation_on_activate', True)
            component.set_force_solo(True)
            component.activate(True)
            # 延迟激活依靠原生TickComponent重试World/PSO就绪；此时不能提前关闭它。
            self.components[stage] = component
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.persist()

    def persist(self):
        """仅保存本次瞬态证据，不修改关卡、正式资产或其它验证报告。"""
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding='utf-8')

    def tick(self, delta):
        """GPU帧提交与异步回读之间分阶段等待；超时失败关闭，不用CPU计算值冒充粒子。"""
        try:
            now = time.monotonic()
            if now - self.started > 40:
                raise RuntimeError('冰箭GPU回读超时')
            if self.phase == 'await_activation':
                for stage, component in self.components.items():
                    if stage in self.ready_stages:
                        continue
                    cache = unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(component)
                    captured = unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                        cache, unreal.NiagaraSimCacheCreateParameters(), component, False, 1 / 30)
                    if captured and cache.get_num_frames() > 0:
                        self.ready_stages.add(stage)
                        component.set_component_tick_enabled(False)
                if len(self.ready_stages) == len(self.components):
                    # 所有真实模拟存在后才统一重置计时，不能把AdvanceSimulation空操作当成已推进。
                    for component in self.components.values():
                        component.activate(True)
                        component.set_component_tick_enabled(False)
                    self.report['activationVerifiedBySimCache'] = sorted(self.ready_stages)
                    self.phase, self.last = 'simulate', now
            elif self.phase == 'simulate' and now - self.last >= 1 / 30:
                for component in self.components.values():
                    component.advance_simulation(1, 1 / 30)
                self.steps += 1
                self.last = now
                if self.steps == 3:
                    self.caches, self.tasks = {}, []
                    for stage, component in self.components.items():
                        cache = unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(component)
                        self.caches[stage] = cache
                        self.tasks.append(unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                            cache, unreal.NiagaraSimCacheCreateParameters(), component, False, 1 / 30))
                    self.phase, self.mark = 'readback', now
            elif self.phase == 'readback' and now - self.mark > 0.5:
                row = {'sample': self.sample, 'requestedSimulationSeconds': (self.sample + 1) * 0.1, 'systems': {}}
                for stage, cache in self.caches.items():
                    assert cache.get_num_frames() > 0, 'GPU缓存没有帧：' + stage
                    emitters = {}
                    for index in range(cache.get_num_emitters()):
                        emitter = str(cache.get_emitter_name(index))
                        colors = list(cache.read_color_attribute('Color', emitter, 0))
                        positions = list(cache.read_position_attribute('Position', emitter, False, 0))
                        alphas = [color.a for color in colors]
                        xyz = [[p.x, p.y, p.z] for p in positions]
                        assert len(colors) == len(positions), 'GPU颜色/位置粒子数量不一致'
                        assert all(math.isfinite(value) for point in xyz for value in point) and all(math.isfinite(a) for a in alphas)
                        emitters[emitter] = {'count': len(colors), 'alpha': alphas, 'positionsCm': xyz,
                                             'maxAlpha': max(alphas, default=0)}
                    row['systems'][stage] = {'simCacheStartSeconds': cache.get_start_seconds(), 'emitters': emitters}
                self.report['samples'].append(row)
                self.persist()
                if self.sample == 0:
                    self.sample, self.steps, self.phase, self.last = 1, 0, 'simulate', now
                else:
                    # 只判定每个系统至少一层存在真实可见Alpha；具体层级尺寸和画面仍需人工验收。
                    self.report['allSystemsHaveVisibleParticles'] = all(
                        any(emitter['maxAlpha'] > 0.01 for sample in self.report['samples']
                            for emitter in sample['systems'][stage]['emitters'].values())
                        for stage in self.components)
                    if self.capture_viewport and self.report['allSystemsHaveVisibleParticles']:
                        self.request_viewport_frame(now)
                    else:
                        self.finish('passed' if self.report['allSystemsHaveVisibleParticles'] else 'failed')
            elif self.phase == 'capture' and now - self.mark >= 2.0:
                if not self.pending_frame.is_file():
                    if now - self.mark > 25:
                        raise RuntimeError('真实冰箭视口截图未落盘')
                    return
                raw = self.pending_frame.read_bytes()
                assert len(raw) > 2000, '真实冰箭视口截图异常小'
                self.report['viewportFrame'] = {'path': str(self.pending_frame), 'bytes': len(raw),
                                                'sha256': hashlib.sha256(raw).hexdigest()}
                self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.finish('failed')

    def request_viewport_frame(self, now):
        """在GPU采样后冻结当前模拟，用真实世界相机拍摄Flight；仅创建/清理本探针临时Actor。"""
        assert 'Flight' in self.components, '真实视口审核需要Flight系统'
        component = self.components['Flight']
        origin = component.get_world_location()
        camera = self.api.spawn_actor_from_class(unreal.CameraActor, origin + unreal.Vector(260, -450, 150), transient=True)
        self.actors.append(camera)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(
            camera.get_actor_location(), origin + unreal.Vector(-50, 0, 0)), False)
        camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(48)
        # Automation截图读取活动编辑器视口；仅传camera参数不足以切换当前审核相机。
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        levels.pilot_level_actor(camera)
        levels.editor_set_viewport_realtime(True)
        levels.editor_invalidate_viewports()
        self.pending_frame = self.path.with_suffix('.png')
        assert not self.pending_frame.exists(), '拒绝读取旧冰箭截图'
        task = unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, str(self.pending_frame),
            camera=camera, mask_enabled=False, capture_hdr=False, delay=0.15, force_game_view=True)
        assert task is not None, '真实视口截图任务未建立'
        self.tasks.append(task)
        self.phase, self.mark = 'capture', now

    def finish(self, status):
        """任何终态只清理本探针拥有的回调与临时Actor，不保存当前人工关卡。"""
        unreal.unregister_slate_post_tick_callback(self.handle)
        for actor in self.actors:
            if unreal.SystemLibrary.is_valid(actor):
                self.api.destroy_actor(actor)
        self.report['status'] = status
        self.report['elapsedSeconds'] = time.monotonic() - self.started
        self.persist()
        print('ICE_GPU_PROBE', status, str(self.path))


unreal._ice_arrow_gpu_probe = IceArrowGPUProbe(
    globals().get('ICE_GPU_STAGES', ['Cast', 'GroundRune', 'Launch', 'Flight', 'Impact', 'FrostResidue']),
    globals().get('ICE_GPU_REPORT_NAME', 'GPUProbe'), globals()['ICE_GPU_EDITOR_PID'],
    capture_viewport=globals().get('ICE_GPU_CAPTURE_VIEWPORT', False))
