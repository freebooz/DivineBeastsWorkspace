"""通过正式编辑器捕获当前真实新手村底色俯视图，不修改关卡及玩家出生点。

第三层离线制作工具只拥有瞬态镜头和渲染目标。输出PNG供Monolith UI导入，
不直接创建任何界面资产。正北为世界+X，正东为+Y，范围与运行时投影一致。
使用独立捕获组件的无光照FinalColorLDR，保留真实地形材质，不填入示意森林。
不修改世界光照和天气。导入纹理必须通过Monolith设置CompressionNoAlpha，
运行格式采用不透明RGB；不能把全零Alpha作为地图遮罩。圆窗由UI画刷独立裁剪。
完成或异常均解除自有帧回调并销毁镜头；证据不代表实时小地图或联机验收。
"""
from configparser import ConfigParser
import hashlib
import json
import math
from pathlib import Path
import re
import unreal


def map_rect(project):
    """读取客户端投影使用的项目配置，返回世界厘米中心和底图边长。"""
    config = ConfigParser(interpolation=None, strict=False)
    config.read(project / 'Config/DefaultGame.ini', encoding='utf-8-sig')
    settings = config['/Script/DivineBeastsUIClient.DivineBeastsUISettings']
    center = re.fullmatch(r'\(X=([^,]+),\s*Y=([^,]+)\)', settings['VillageMinimapCenterCm'])
    if center is None:
        raise ValueError('小地图中心须采用 (X=…,Y=…) 配置')
    x, y = map(float, center.groups())
    width = settings.getfloat('VillageMinimapWidthCm')
    if not all(map(math.isfinite, (x, y, width))) or width <= 0:
        raise ValueError('小地图中心或范围非法')
    return x, y, width


def capture(output_override=None):
    """只允许真实新手村已加载时捕获；完成标识与源图复核后才能导入。"""
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).get_current_level()
    if not level or 'L_Village_Start' not in level.get_path_name():
        raise RuntimeError('当前关卡不是正式新手村，拒绝生成误导底图')
    project = Path(unreal.Paths.project_dir()).resolve()
    root = project.parent
    x, y, width = map_rect(project)
    output = Path(output_override) if output_override else project / 'Saved/Validation/AnimationHUD/VillageMinimapCapture.png'
    output = output.resolve()
    if not output.is_relative_to(root):
        raise RuntimeError('捕获输出必须位于当前工程内')
    output.parent.mkdir(parents=True, exist_ok=True)
    selection = list(actors.get_selected_level_actors())
    camera = actors.spawn_actor_from_class(unreal.SceneCapture2D,
        unreal.Vector(x, y, 55000), unreal.Rotator(pitch=-90, yaw=0, roll=0), transient=True)
    component = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    # 使用正式创建API初始化RHI资源，避免依赖裸UObject的编辑属性副作用。
    target = unreal.RenderingLibrary.create_render_target2d(camera.get_world(), 1024, 1024,
        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB, unreal.LinearColor(.025, .025, .025, 1), False)
    if not camera or not target:
        raise RuntimeError('无法创建瞬态捕获组件或渲染目标')
    component.set_editor_property('texture_target', target)
    component.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    component.set_editor_property('ortho_width', width)
    component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    component.set_editor_property('capture_every_frame', False)
    settings = []
    for name in ('Atmosphere', 'Fog', 'VolumetricFog', 'Lighting', 'PostProcessing', 'MotionBlur', 'TemporalAA', 'Bloom'):
        setting = unreal.EngineShowFlagsSetting()
        setting.set_editor_property('show_flag_name', name)
        setting.set_editor_property('enabled', False)
        settings.append(setting)
    component.set_editor_property('show_flag_settings', settings)
    state = {'phase': 0, 'elapsed': 0.0, 'handle': None}

    def finish():
        """失败和完成采用同一释放顺序，不留下镜头或编辑器帧任务。"""
        if state['handle'] is not None:
            unreal.unregister_slate_post_tick_callback(state['handle'])
            state['handle'] = None
        if unreal.SystemLibrary.is_valid(camera):
            actors.destroy_actor(camera)
        actors.set_selected_level_actors([actor for actor in selection if unreal.SystemLibrary.is_valid(actor)])

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
                    'sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
                    'center_centimeters': [x, y], 'extent_centimeters': width,
                    'north': '+X', 'east': '+Y', 'capture_source': 'BaseColor',
                    'capture_mode': 'FinalColorLDR_Unlit', 'world_modified': False}
                output.with_suffix('.json').write_text(json.dumps(evidence, ensure_ascii=False, indent=2), encoding='utf-8')
                print('REAL_VILLAGE_MINIMAP_CAPTURED')
                finish()
        except Exception:
            finish()
            raise

    state['handle'] = unreal.register_slate_post_tick_callback(tick)
    print('REAL_VILLAGE_MINIMAP_CAPTURE_SCHEDULED')
