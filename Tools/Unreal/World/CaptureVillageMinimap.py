"""通过正式编辑器捕获当前真实新手村底色俯视图，不修改关卡及玩家出生点。

第三层离线制作工具只拥有瞬态镜头和渲染目标。输出PNG供Monolith UI导入，
不直接创建任何界面资产。正北为世界+X，正东为+Y，范围与运行时投影一致。
使用BaseColor去除俯视阴影和曝光污染，保留真实地形材质，不填入示意森林。
BaseColor捕获的Alpha不含覆盖率，导入纹理必须通过Monolith设置CompressionNoAlpha，
运行格式采用不透明RGB；不能把全零Alpha作为地图遮罩。圆窗由UI画刷独立裁剪。
完成或异常均解除自有帧回调并销毁镜头；证据不代表实时小地图或联机验收。
"""
import json
from pathlib import Path
import unreal


def capture():
    """只允许真实新手村已加载时捕获；完成标识与源图复核后才能导入。"""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    if not level or 'L_Village_Start' not in level.get_path_name():
        raise RuntimeError('当前关卡不是正式新手村，拒绝生成误导底图')
    root = Path(unreal.Paths.project_dir()).resolve().parent
    output = root / 'Saved/Validation/AnimationHUD/VillageMinimapCapture.png'
    output.parent.mkdir(parents=True, exist_ok=True)
    camera = actors.spawn_actor_from_class(unreal.SceneCapture2D,
        unreal.Vector(0, 0, 30000), unreal.Rotator(pitch=-90, yaw=0, roll=0), transient=True)
    component = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    target = unreal.TextureRenderTarget2D()
    # BaseColor是线性值；sRGB目标执行唯一的线性到显示编码，避免把线性暗值误当显示RGB。
    target.set_editor_property('render_target_format', unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    target.set_editor_property('size_x', 1024)
    target.set_editor_property('size_y', 1024)
    target.set_editor_property('clear_color', unreal.LinearColor(.15, .15, .15, 1))
    component.set_editor_property('texture_target', target)
    component.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    component.set_editor_property('ortho_width', 50400)
    component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_BASE_COLOR)
    component.set_editor_property('capture_every_frame', False)
    component.set_editor_property('show_flag_settings', [
        unreal.EngineShowFlagsSetting(show_flag_name=name, enabled=False)
        for name in ('Atmosphere', 'Fog', 'VolumetricFog')])
    state = {'phase': 0, 'elapsed': 0.0, 'handle': None}

    def finish():
        """失败和完成采用同一释放顺序，不留下镜头或编辑器帧任务。"""
        if state['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
        if unreal.SystemLibrary.is_valid(camera):
            actors.destroy_actor(camera)

    def tick(delta_seconds):
        """仅离线捕获分帧等待渲染注册；不属于游戏UI的逐帧轮询。"""
        state['elapsed'] += delta_seconds
        if state['elapsed'] < .5:
            return
        state['elapsed'] = 0
        try:
            if state['phase'] == 0:
                component.capture_scene()
                state['phase'] = 1
            else:
                unreal.RenderingLibrary.export_render_target(camera.get_world(), target, str(output.parent), output.name)
                evidence = {'level': level.get_path_name(), 'source': str(output), 'size': [1024, 1024],
                    'extent_centimeters': 50400, 'north': '+X', 'east': '+Y', 'capture_source': 'BaseColor',
                    'world_modified': False}
                output.with_suffix('.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
                print('REAL_VILLAGE_MINIMAP_CAPTURED')
                finish()
        except Exception:
            finish()
            raise

    state['handle'] = unreal.register_slate_post_tick_callback(tick)
    print('REAL_VILLAGE_MINIMAP_CAPTURE_SCHEDULED')
