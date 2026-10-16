#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 冰箭真实视口审核夹具，由 Monolith run_python 调用。

所属项目工具层，仅在带 IceArrowOwnedAuthoring 标记的编辑器中工作。
创建临时审核世界，固定光照、相机及曝光；每个 Slate 帧推进一次 Niagara，
让 GPU 有真实渲染机会。截图来自原生视口，不以离线概念图代替渲染。
不保存正式资源、不改变技能判定；结束或异常时注销回调，保留世界供复核。
"""
from pathlib import Path
import json
import time
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / 'Saved/Validation/IceArrow/Production_20261016'
BASE = '/DBAPresentationPack_Core/VFX/IceArrow'


class IceArrowViewportReview:
    """拥有本次临时世界中的审核组件和回调；不接管已有游戏世界。"""

    def __init__(self, stage='Flight', name='Flight_ViewportBaseline', age=0.35, asset_path=None, moving=False, oblique=False, world_path=None):
        assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
        assert unreal.Paths.get_project_file_path().replace('\\', '/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
        old = getattr(unreal, '_ice_viewport_review', None)
        if old and old.handle:
            raise RuntimeError('上一轮审核尚未结束，禁止重入')
        self.handle = None
        self.stage, self.name, self.age = stage, name, age
        self.moving = moving
        self.started = time.monotonic()
        self.frame = 0
        self.steps = 0
        self.simulation_started = False
        self.capture_task = None
        self.capture_started = None
        self.primed = False
        self.output = OUT / (name + '.png')
        if self.output.exists():
            raise RuntimeError('本轮截图名称已存在，拒绝沿用旧图')
        self.editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        if world_path:
            assert self.levels.load_level(world_path), '现有场景加载失败'
            self.world = self.editor.get_editor_world()
        else:
            self.world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.report = {'status': '运行中', 'stage': stage, 'age_seconds': age,
                       'source': 'UE5.8 原生视口逐帧 GPU 渲染', 'human_accepted': False, 'existing_scene': world_path, 'game_runtime_verified': False}
        # 已有场景仅添加临时审核对象，保留其光照与场景，不保存原地图。
        if not world_path:
            floor = self.spawn(unreal.StaticMeshActor, 'IceReview_Floor', unreal.Vector(0, 0, -5))
            floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
            floor.static_mesh_component.set_material(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
            ground_material = floor.static_mesh_component.create_dynamic_material_instance(0)
            ground_material.set_vector_parameter_value('Color', unreal.LinearColor(.035, .045, .060, 1))
            floor.set_actor_scale3d(unreal.Vector(20, 20, 1))
            light = self.spawn(unreal.DirectionalLight, 'IceReview_Key', unreal.Vector(0, 0, 500))
            light.set_actor_rotation(unreal.Rotator(-45, 35, 0), False)
            light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', 4.0)
            fill = self.spawn(unreal.DirectionalLight, 'IceReview_Fill', unreal.Vector(0, 0, 500))
            fill.set_actor_rotation(unreal.Rotator(-35, -145, 0), False)
            fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity', 1.2)
            sky = self.spawn(unreal.SkyLight, 'IceReview_Ambient', unreal.Vector(0, 0, 400))
            sky_component = sky.get_component_by_class(unreal.SkyLightComponent)
            sky_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
            sky_component.set_editor_property('cubemap', unreal.load_asset('/Engine/EngineResources/DefaultTextureCube'))
            sky_component.set_editor_property('intensity', .6)
        self.camera = self.spawn(unreal.CameraActor, 'IceReview_Camera', unreal.Vector(-260, 420, 230) if oblique else unreal.Vector(-60, 470, 210))
        self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(self.camera.get_actor_location(), unreal.Vector(-60, 0, 90)), False)
        camera_component = self.camera.get_component_by_class(unreal.CameraComponent)
        camera_component.set_field_of_view(55)
        self.report['camera_variant'] = '斜视固定机位' if oblique else '原始侧视固定机位'
        pp = camera_component.get_editor_property('post_process_settings')
        for key, value in {'override_auto_exposure_method': True, 'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
                           'override_auto_exposure_bias': True, 'auto_exposure_bias': 0.0,
                           'override_auto_exposure_apply_physical_camera_exposure': True, 'auto_exposure_apply_physical_camera_exposure': False,
                           'override_motion_blur_amount': True, 'motion_blur_amount': 0.0,
                           'override_bloom_intensity': True, 'bloom_intensity': 0.25}.items():
            pp.set_editor_property(key, value)
        camera_component.set_editor_property('post_process_settings', pp)
        actor = self.spawn(unreal.NiagaraActor, 'IceReview_' + stage, unreal.Vector(0, 0, 100 if stage != 'Cast' else 0))
        self.effect_actor = actor
        self.component = actor.get_component_by_class(unreal.NiagaraComponent)
        self.component.set_asset(unreal.load_asset(asset_path or BASE + '/Niagara/NS_DBA_IceArrow_' + stage))
        self.component.set_editor_property('allow_scalability', False)
        self.component.set_force_solo(True)
        self.component.activate(True)
        # 编辑器世界由唯一手动时钟驱动，避免组件自动更新与审核时钟叠加。
        self.component.set_component_tick_enabled(False)
        self.levels.pilot_level_actor(self.camera)
        self.levels.editor_set_viewport_realtime(True)
        self.levels.editor_set_game_view(True)
        unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.persist()

    def spawn(self, cls, label, location):
        """生成仅供当前审核世界使用的 Actor；所有权随临时世界释放。"""
        actor = self.actors.spawn_actor_from_class(cls, location)
        actor.set_actor_label(label)
        return actor

    def persist(self):
        """只保存审核状态文本；不将未验证的世界或 Niagara 保存为正式资源。"""
        (OUT / (self.name + '.json')).write_text(json.dumps(self.report, ensure_ascii=False, indent=2), encoding='utf-8')

    def finish(self, status):
        """注销回调，阻止异常后继续更新；组件留在审核世界便于人工观察。"""
        if self.handle:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle = None
        self.report['status'] = status
        self.report['frames'] = self.frame
        self.persist()

    def tick(self, delta_seconds):
        """逐渲染帧推进模拟，冻结目标时刻后异步截图；超时明确失败。"""
        try:
            self.frame += 1
            if time.monotonic() - self.started > 60:
                raise RuntimeError('视口审核超时')
            if time.monotonic() - self.started < 3:
                return
            if not self.simulation_started:
                self.component.reinitialize_system()
                self.component.set_component_tick_enabled(False)
                self.report['active_after_reset'] = self.component.is_active()
                self.report['step_states'] = []
                self.simulation_started = True
            if self.steps < round(self.age * 60):
                if self.moving:
                    # 仅审核夹具推动视觉组件；逐步生成世界空间历史点，保持终点主体在原机位。
                    x = (self.steps / 60.0 - self.age) * 720.0
                    self.effect_actor.set_actor_location(unreal.Vector(x, 0, 100), False, False)
                self.component.advance_simulation(1, 1.0 / 60.0)
                self.steps += 1
                self.report['step_states'].append({'step': self.steps, 'active': self.component.is_active()})
                return
            if self.capture_task is None:
                if self.frame < 40 + self.steps:
                    return
                # 离屏编辑器首次高分截图用于建立真实视图和 GPU 渲染资源，验收只读后续帧。
                target = self.output if self.primed else self.output.with_name(self.output.stem + '_Warmup.png')
                self.capture_task = unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, str(target), camera=self.camera, mask_enabled=False, capture_hdr=False, delay=0.15, force_game_view=True)
                if self.capture_task is None:
                    raise RuntimeError('未建立原生截图任务')
                self.capture_started = time.monotonic()
                return
            if not self.primed and time.monotonic() - self.capture_started > 2:
                self.primed = True
                self.capture_task = None
                return
            if self.output.exists() and time.monotonic() - self.capture_started > 2:
                self.report['image'] = str(self.output)
                self.finish('已截图，待视觉检查')
        except Exception:
            self.report['error'] = traceback.format_exc()
            self.finish('失败')


def start(stage='Flight', name='Flight_ViewportBaseline', age=0.35, asset_path=None, moving=False, oblique=False, world_path=None):
    """由 Monolith 调用并保留实例，确保异步 Slate 回调期间对象不会回收。"""
    unreal._ice_viewport_review = IceArrowViewportReview(stage, name, age, asset_path, moving, oblique, world_path)


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage', default='Flight')
    parser.add_argument('--name', required=True)
    parser.add_argument('--age', type=float, default=.35)
    parser.add_argument('--asset')
    parser.add_argument('--moving', action='store_true')
    parser.add_argument('--oblique', action='store_true')
    parser.add_argument('--world')
    arguments = parser.parse_args()
    start(arguments.stage, arguments.name, arguments.age, arguments.asset, arguments.moving, arguments.oblique, arguments.world)
