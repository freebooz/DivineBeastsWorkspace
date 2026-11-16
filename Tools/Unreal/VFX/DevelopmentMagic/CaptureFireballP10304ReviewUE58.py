# -*- coding: utf-8 -*-
"""SSLM项目层P1—03/04原生PIE固定镜头录帧，仅由Monolith在本次Editor内启动。

消费真实Manny施法动画的手部Socket和新审核Niagara，不绑定正式技能，不修改地图或权威时长。
模拟进度只属于独立审核；逐帧读骨骼属于临时TA测试，不是新的运行时播放器或UI轮询。
输出真实SceneCapture PNG、SimCache位置/尺寸/粒子数及有明确边界的原生推进墙钟耗时。
结束/异常均只清理本次PIE与Slate回调，不退出编辑器、不保存临时世界。
"""
from pathlib import Path
import unreal as u
import json
import time
import math
import traceback
from datetime import datetime, timezone

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
OUT = ROOT / 'Saved/Validation/FireballV3P10304_20261026'
BASE = '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/'
PATHS = {'03': BASE + 'NS_DBA_Fireball_P1_03_StreamConverge_Review',
         '04': BASE + 'NS_DBA_Fireball_P1_04_CoreFormed_Review'}


def vec(v):
    return [float(v.x), float(v.y), float(v.z)]


class Capture:
    """拥有一个临时PIE和两份审核组件；出错保留证据，不碰其它世界或资产。"""
    def __init__(self):
        assert Path(u.Paths.get_project_file_path()).resolve() == ROOT / 'Game/DivineBeastsArena.uproject'
        assert u.SystemLibrary.get_engine_version().startswith('5.8')
        assert not u.EditorLevelLibrary.get_pie_worlds(False), '已有PIE，不能接管'
        assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '未保存地图必须保留'
        assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '未保存资产必须保留'
        self.run = OUT / 'NativeCapture' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        (self.run / 'Frames').mkdir(parents=True, exist_ok=False)
        self.result = {'status': 'initializing', 'engine': u.SystemLibrary.get_engine_version(),
                       'project': u.Paths.get_project_file_path(), 'source': 'UE5.8 D3D12 PIE SceneCapture FinalColorLDR',
                       'resolution': [1920, 1080], 'videoFPS': 30, 'frames': [], 'samples': [], 'advanceMs': {'03': [], '04': []},
                       'authorityChanged': False, 'productionBound': False, 'manualAccepted': False,
                       'performanceCaveat': 'advanceMs为force-solo组件原生AdvanceSimulation的调用墙钟耗时，含调度/包装；不等于独占GPU耗时。',
                       'errors': [], 'camera': {'position': [390, -515, 263], 'lookAt': [0, 0, 117], 'fov': 48, 'exposureBias': .7},
                       'rig': {'leftSocket': 'hand_l', 'rightSocket': 'hand_r', 'animationPositionSeconds': .28,
                               'convergenceForwardCm': 58, 'centerOwner': 'temporary review rig, not hardcoded in Niagara'}}
        self.editor = u.get_editor_subsystem(u.EditorActorSubsystem)
        self.levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
        self.world = u.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.world.get_world_settings().set_editor_property('default_game_mode', u.GameModeBase)
        self.state = 'wait_pie'; self.started = time.monotonic(); self.last = self.started
        self.frame = 0; self.active_stage = None; self.handle = None
        self.assets = {k: u.load_asset(p) for k, p in PATHS.items()}
        assert all(isinstance(a, u.NiagaraSystem) for a in self.assets.values())
        floor = self.spawn(u.StaticMeshActor, 'Floor', (0, 0, -5))
        floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Plane'))
        floor.set_actor_scale3d(u.Vector(20, 20, 1))
        for label, pitch, yaw, intensity in [('Key', -44, -31, 7.0), ('Fill', -32, 100, 2.5)]:
            light = self.spawn(u.DirectionalLight, label, (0, 0, 500))
            light.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw, roll=0), False)
            light.get_component_by_class(u.DirectionalLightComponent).set_editor_property('intensity', intensity)
        sky = self.spawn(u.SkyLight, 'Sky', (0, 0, 350))
        sc = sky.get_component_by_class(u.SkyLightComponent)
        sc.set_editor_property('source_type', u.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sc.set_editor_property('cubemap', u.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        sc.set_editor_property('intensity', .7)
        hero = self.spawn(u.SkeletalMeshActor, 'Caster', (0, 0, 0))
        hero.set_actor_rotation(u.Rotator(pitch=0, yaw=145, roll=0), False)
        mesh = hero.get_component_by_class(u.SkeletalMeshComponent)
        mesh.set_skeletal_mesh_asset(u.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
        mesh.set_animation_mode(u.AnimationMode.ANIMATION_SINGLE_NODE)
        mesh.set_animation(u.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast'))
        mesh.set_position(.28, False)
        for stage in PATHS:
            actor = self.spawn(u.NiagaraActor, stage, (0, 0, 100))
            comp = actor.get_component_by_class(u.NiagaraComponent)
            comp.set_auto_activate(False)
            comp.set_asset(self.assets[stage])
            comp.set_editor_property('allow_scalability', False)
        camera = self.spawn(u.SceneCapture2D, 'Camera', (390, -515, 263))
        camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(u.Vector(390, -515, 263), u.Vector(0, 0, 117)), False)
        comp = camera.get_component_by_class(u.SceneCaptureComponent2D)
        comp.set_editor_property('capture_every_frame', False)
        comp.set_editor_property('capture_on_movement', False)
        comp.set_editor_property('capture_source', u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property('fov_angle', 48.0)
        pp = comp.get_editor_property('post_process_settings')
        for key, value in {'override_auto_exposure_method': True, 'auto_exposure_method': u.AutoExposureMethod.AEM_MANUAL,
                           'override_auto_exposure_bias': True, 'auto_exposure_bias': .7,
                           'override_auto_exposure_apply_physical_camera_exposure': True,
                           'auto_exposure_apply_physical_camera_exposure': False,
                           'override_motion_blur_amount': True, 'motion_blur_amount': 0.0,
                           'override_bloom_intensity': True, 'bloom_intensity': .22}.items():
            pp.set_editor_property(key, value)
        comp.set_editor_property('post_process_settings', pp)
        self.handle = u.register_slate_post_tick_callback(self.tick)
        self.persist()
        self.levels.editor_request_begin_play()
        print('FIREBALL_P10304_NATIVE_CAPTURE_STARTED', str(self.run), flush=True)

    def spawn(self, cls, label, pos):
        """仅在本次空白审核地图生成可识别Actor。"""
        a = self.editor.spawn_actor_from_class(cls, u.Vector(*pos))
        assert a, label
        a.set_actor_label('P10304_Review_' + label)
        return a

    def persist(self):
        (self.run / 'Result.json').write_text(json.dumps(self.result, ensure_ascii=False, indent=2), encoding='utf-8')
        (OUT / 'LatestCapture.json').write_text(json.dumps({'run': str(self.run), 'status': self.result['status']}, indent=2), encoding='utf-8')

    def points(self):
        """从实际骨骼读取双掌；中心由审核Rig配置产生，Niagara不认识Manny骨架。"""
        left = self.mesh.get_socket_location('hand_l'); right = self.mesh.get_socket_location('hand_r')
        mid = (left + right) * .5
        chest = self.mesh.get_socket_location('spine_03')
        forward = u.Vector(mid.x - chest.x, mid.y - chest.y, 0)
        length = math.sqrt(forward.x * forward.x + forward.y * forward.y)
        if length < .1:
            forward = self.hero.get_actor_forward_vector(); length = max(.1, math.sqrt(forward.x * forward.x + forward.y * forward.y))
        forward = forward / length
        center = mid + forward * self.result['rig']['convergenceForwardCm']
        return left, right, center

    def drive(self, stage, phase, quality=3):
        """合法Position接口处理大世界坐标；一个阶段只激活一个审核组件。"""
        left, right, center = self.points()
        c = self.components[stage]
        c.get_owner().set_actor_location(center, False, False)
        c.set_variable_position('User.LeftPalmWS', left)
        c.set_variable_position('User.RightPalmWS', right)
        c.set_variable_position('User.ConvergeCenterWS', center)
        c.set_variable_float('User.PhaseProgress', phase)
        c.set_variable_float('User.FlameScale', 1.0)
        c.set_variable_float('User.FlameIntensity', 1.0)
        c.set_variable_int('User.QualityLevel', quality)
        if self.active_stage != stage:
            for other in self.components.values():
                other.deactivate(); other.set_paused(True); other.set_visibility(False, True)
            c.set_force_solo(True)
            c.set_paused(False); c.set_visibility(True, True)
            c.activate(True)
            c.set_component_tick_enabled(False)
            self.active_stage = stage
        start = time.perf_counter_ns()
        c.advance_simulation(1, 1.0 / 30.0)
        self.result['advanceMs'][stage].append((time.perf_counter_ns() - start) / 1.e6)
        self.current_points = {'left': vec(left), 'right': vec(right), 'center': vec(center)}
        return c

    def sample(self, c, stage, phase):
        """从真实Niagara SimCache读取结果，拒绝把计划粒子数或手算轨迹当成运行证据。"""
        cache = u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(c)
        assert u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache, u.NiagaraSimCacheCreateParameters(), c, False, 1.0 / 30.0)
        row = {'frame': self.frame, 'stage': stage, 'phase': phase, 'points': self.current_points, 'emitters': {}}
        for i in range(cache.get_num_emitters()):
            name = str(cache.get_emitter_name(i))
            pos = list(cache.read_position_attribute('Position', name, True, 0))
            colors = list(cache.read_color_attribute('Color', name, 0))
            scales = list(cache.read_vector_attribute('Scale', name, 0))
            sizes = list(cache.read_vector2_attribute('SpriteSize', name, 0))
            row['emitters'][name] = {'count': len(pos), 'firstPosition': vec(pos[0]) if pos else None,
                'lastPosition': vec(pos[-1]) if pos else None, 'positions': [vec(p) for p in pos],
                'scale': [vec(s) for s in scales], 'alpha': [float(x.a) for x in colors],
                'spriteSize': [[float(x.x), float(x.y)] for x in sizes]}
        self.result['samples'].append(row)

    def finish(self, status):
        """只结束自己拥有的临时PIE；不保存地图，不退出编辑器，异常保留原生证据。"""
        self.result['status'] = status
        self.persist()
        if self.handle:
            u.unregister_slate_post_tick_callback(self.handle); self.handle = None
        if hasattr(self, 'components'):
            for c in self.components.values():
                if u.SystemLibrary.is_valid(c):
                    c.deactivate(); c.set_paused(True); c.set_visibility(False, True)
        self.levels.editor_request_end_play()
        print('FIREBALL_P10304_NATIVE_CAPTURE_FINISHED', status, str(self.run), flush=True)

    def tick(self, dt):
        """UI帧回调仅推进TA审核；截图导出完成后才进入下个确定性模拟步。"""
        try:
            if time.monotonic() - self.started > 240: raise TimeoutError('审核捕获超时')
            if self.state == 'wait_pie':
                worlds = u.EditorLevelLibrary.get_pie_worlds(False)
                if not worlds: return
                assert len(worlds) == 1
                self.pie = worlds[0]
                actors = u.GameplayStatics.get_all_actors_of_class
                self.hero = [a for a in actors(self.pie, u.SkeletalMeshActor) if a.get_actor_label() == 'P10304_Review_Caster'][0]
                self.mesh = self.hero.get_component_by_class(u.SkeletalMeshComponent)
                self.mesh.set_position(.28, False)
                self.components = {a.get_actor_label()[-2:]: a.get_component_by_class(u.NiagaraComponent)
                                   for a in actors(self.pie, u.NiagaraActor) if a.get_actor_label().startswith('P10304_Review_')}
                assert set(self.components) == {'03', '04'}
                self.camera = [a for a in actors(self.pie, u.SceneCapture2D) if a.get_actor_label() == 'P10304_Review_Camera'][0]
                self.capture = self.camera.get_component_by_class(u.SceneCaptureComponent2D)
                self.target = u.RenderingLibrary.create_render_target2d(self.pie, 1920, 1080, u.TextureRenderTargetFormat.RTF_RGBA8)
                self.capture.set_editor_property('texture_target', self.target)
                self.state = 'warm'; self.last = time.monotonic(); return
            if self.state == 'warm':
                if time.monotonic() - self.last < .8: return
                self.state = 'step'; return
            if self.state == 'step':
                if self.frame >= 106:
                    self.finish('captured'); return
                stage = '03' if self.frame <= 45 else '04'
                phase = self.frame / 45.0 if stage == '03' else min(1.0, (self.frame - 46) / 51.0)
                c = self.drive(stage, phase)
                if self.frame in (0, 15, 30, 45, 46, 60, 75, 90, 97, 105): self.sample(c, stage, phase)
                self.current_stage = stage; self.current_phase = phase
                self.capture.capture_scene()
                self.state = 'export'; return
            if self.state == 'export':
                name = 'Frame_%04d.png' % self.frame
                u.RenderingLibrary.export_render_target(self.pie, self.target, str(self.run / 'Frames'), name)
                image = self.run / 'Frames' / name
                assert image.is_file() and image.stat().st_size > 5000, '原生图像导出失败'
                self.result['frames'].append({'index': self.frame, 'stage': self.current_stage,
                    'phase': self.current_phase, 'file': str(image), 'bytes': image.stat().st_size})
                self.frame += 1
                if self.frame % 15 == 0: self.persist()
                self.state = 'step'
        except Exception:
            self.result['errors'].append(traceback.format_exc())
            self.finish('failed')


def start():
    """显式启动入口；导入模块不自动切换地图或启动PIE。"""
    previous = getattr(u, '_fireball_p10304_capture', None)
    assert not previous or previous.handle is None, '已有本次审核捕获正在运行'
    u._fireball_p10304_capture = Capture()
