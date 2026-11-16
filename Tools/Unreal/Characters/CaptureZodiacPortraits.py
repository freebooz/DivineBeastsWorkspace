"""在正式编辑器中拍摄十二生肖当前真实开发外观；不改地图，不生成虚构英雄模型。

第三层制作工具读取各英雄外观定义，仅创建瞬态演员、光源和渲染目标。
输出 PNG 归各英雄内容包 SourceArt，后续只能由 Monolith UI 导入头像资产。
异常时清理演员；每张图记录实际模型和材质来源，不能视为正式美术验收。
"""
import hashlib
import json
import math
from pathlib import Path
import unreal


def capture(heroes=None, output_root=None, evidence_dir=None):
    """通过编辑器帧回调拍摄；等待组件进入渲染场景，避免同步创建后得到空背景。

    返回仅表示任务已登记；完成标识及源图目视复核才可作为导入依据。
    """
    workspace = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    available = ['Rat', 'Ox', 'Tiger', 'Rabbit', 'Dragon', 'Snake', 'Horse',
                 'Goat', 'Monkey', 'Rooster', 'Dog', 'Boar']
    heroes = list(available if heroes is None else heroes)
    if not heroes or len(set(heroes)) != len(heroes) or any(h not in available for h in heroes):
        raise ValueError('必须指定非重复的已登记英雄')
    output_root = Path(output_root).resolve() if output_root else None
    evidence_dir = Path(evidence_dir).resolve() if evidence_dir else workspace / 'Saved/Validation/AnimationHUD'
    for output_path in (output_root, evidence_dir):
        if output_path and not output_path.is_relative_to(workspace):
            raise ValueError('头像制作输出必须在当前工作空间内')
    evidence_dir.mkdir(parents=True, exist_ok=True)
    created = []
    records = []
    origin = unreal.Vector(0, 0, 10000)
    try:
        actor = subsystem.spawn_actor_from_class(unreal.SkeletalMeshActor, origin, transient=True)
        created.append(actor)
        mesh = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        lights = []
        for offset, intensity, color in [((160, -120, 200), 60, (1, .97, .94)),
                                          ((80, 150, 160), 35, (.8, .88, 1))]:
            light = subsystem.spawn_actor_from_class(unreal.PointLight,
                origin + unreal.Vector(*offset), transient=True)
            created.append(light)
            lights.append(light)
            component = light.get_component_by_class(unreal.PointLightComponent)
            component.set_editor_property('intensity', intensity)
            component.set_editor_property('attenuation_radius', 1200)
            component.set_editor_property('source_radius', 25)
            component.set_light_color(unreal.LinearColor(*color, 1))
        camera = subsystem.spawn_actor_from_class(unreal.SceneCapture2D,
            origin + unreal.Vector(100, 0, 158), unreal.Rotator(pitch=0, yaw=180, roll=0), transient=True)
        created.append(camera)
        component = camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        target = unreal.TextureRenderTarget2D()
        target.set_editor_property('render_target_format', unreal.TextureRenderTargetFormat.RTF_RGBA8)
        # 固定头肩构图；旧180厘米镜头距离拍成半身，放入小圆框后无法辨认角色面部。
        target.set_editor_property('size_x', 512)
        target.set_editor_property('size_y', 512)
        target.set_editor_property('clear_color', unreal.LinearColor(.025, .03, .03, 1))
        component.set_editor_property('texture_target', target)
        component.set_editor_property('fov_angle', 35)
        component.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        component.set_editor_property('capture_every_frame', False)
        # 固定曝光及关闭雾/天空，头像保留真实材质颜色，不把自动曝光的白色过曝当成肖像。
        settings = unreal.PostProcessSettings()
        for key, value in {'override_auto_exposure_method': True,
                           'auto_exposure_method': unreal.AutoExposureMethod.AEM_MANUAL,
                           'override_auto_exposure_apply_physical_camera_exposure': True,
                           'auto_exposure_apply_physical_camera_exposure': False,
                           'override_auto_exposure_bias': True, 'auto_exposure_bias': -5.0,
                           'override_bloom_intensity': True, 'bloom_intensity': 0.0}.items():
            settings.set_editor_property(key, value)
        component.set_editor_property('post_process_settings', settings)
        component.set_editor_property('post_process_blend_weight', 1.0)
        component.set_editor_property('show_flag_settings', [unreal.EngineShowFlagsSetting(show_flag_name=name, enabled=False)
            for name in ('Atmosphere', 'Fog', 'VolumetricFog')])
        component.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        component.show_only_actor_components(actor)
        globals()['last_stage'] = {'actor': actor, 'mesh': mesh, 'camera': camera, 'capture': component}
        # 动画与网格来自同一个当前外观配置，不能继续给新模型强套旧Mannequin骨架的MM_Idle。
        mesh.set_update_animation_in_editor(True)
        state = {'index': 0, 'phase': 0, 'elapsed': 0.0, 'handle': None}

        def finish():
            """解除帧回调并销毁自有临时对象；失败也遵循同一清理路径。"""
            if state['handle'] is not None:
                unreal.unregister_slate_post_tick_callback(state['handle'])
                state['handle'] = None
            for temporary in reversed(created):
                if unreal.SystemLibrary.is_valid(temporary):
                    subsystem.destroy_actor(temporary)

        def tick(delta_seconds):
            """配置、捕获和导出分帧执行，帧回调仅属于此制作任务而非运行时UI轮询。"""
            state['elapsed'] += delta_seconds
            # 首帧后的纹理驻留需要时间；仅制作回调等待，不是运行时Widget轮询。
            if state['elapsed'] < (2.0 if state['phase'] == 1 else .2):
                return
            state['elapsed'] = 0.0
            try:
                if state['phase'] == 0:
                    configure(heroes[state['index']])
                    state['phase'] = 1
                elif state['phase'] == 1:
                    # 当前项目面向+X；用实际Head骨定位，保持现有UI的头脸UV合同。
                    head_bone = next((name for name in ('Head', 'head', 'mixamorig:Head')
                                      if mesh.get_bone_index(name) >= 0), None)
                    if head_bone is None:
                        raise RuntimeError('当前模型没有可识别的头部骨骼，拒绝沿用旧构图')
                    face = mesh.get_socket_location(head_bone) + unreal.Vector(0, 0, 8)
                    frame_height = 2 * 100 * math.tan(math.radians(35 / 2))
                    camera.set_actor_location(face + unreal.Vector(100, 0, -(0.5 - 0.33) * frame_height), False, True)
                    for light, offset in zip(lights, ((160, -120, 42), (80, 150, 2))):
                        light.set_actor_location(face + unreal.Vector(*offset), False, True)
                    records[-1]['head_bone'] = head_bone
                    records[-1]['face_world_cm'] = [face.x, face.y, face.z]
                    records[-1]['face_uv'] = [.25, .08, .75, .58]
                    state['phase'] = 2
                elif state['phase'] == 2:
                    component.capture_scene()
                    state['phase'] = 3
                else:
                    record = records[-1]
                    output = Path(record['source'])
                    unreal.RenderingLibrary.export_render_target(camera.get_world(), target, str(output.parent), output.name)
                    if not output.is_file():
                        raise RuntimeError('原生头像导出未生成预期PNG: ' + str(output))
                    record['source_sha256'] = hashlib.sha256(output.read_bytes()).hexdigest()
                    state['index'] += 1
                    state['phase'] = 0
                    if state['index'] == len(heroes):
                        evidence = evidence_dir / 'PortraitCapture.json'
                        evidence.write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding='utf-8')
                        print('REAL_CHARACTER_PORTRAITS_CAPTURED=' + str(len(records)))
                        finish()
            except Exception as error:
                (evidence_dir / 'PortraitCapture.Failed.json').write_text(
                    json.dumps({'hero_index': state['index'], 'phase': state['phase'], 'error': str(error)},
                               ensure_ascii=False, indent=2), encoding='utf-8')
                finish()
                raise

        def configure(hero):
            """加载当前英雄拥有的真实模型和材质；每轮清除前一英雄的材质覆盖。"""
            profile_path = '/DBAHeroPack_' + hero + '/Characters/DA_Appearance_Zodiac_' + hero
            profile = unreal.load_asset(profile_path)
            if not profile or str(profile.get_editor_property('hero_definition_id')) != 'Hero.Zodiac.' + hero:
                raise RuntimeError('角色外观配置身份不匹配: ' + hero)
            skeletal_mesh = profile.get_editor_property('skeletal_mesh')
            if not skeletal_mesh:
                raise RuntimeError('当前角色模型未加载: ' + hero)
            mesh.set_skeletal_mesh_asset(skeletal_mesh)
            mesh.set_editor_property('override_materials', [])
            # SkeletalMeshActor的Mesh本身是Root，直接SetRelativeLocation会丢失隔离舞台原点。
            actor.set_actor_location(origin + profile.get_editor_property('mesh_relative_location'), False, True)
            mesh.set_relative_rotation(profile.get_editor_property('mesh_relative_rotation'), False, True)
            mesh.set_relative_scale3d(profile.get_editor_property('mesh_relative_scale'))
            anim_class = profile.get_editor_property('anim_instance_class')
            if not anim_class:
                raise RuntimeError('当前角色动画类缺失: ' + hero)
            mesh.set_anim_instance_class(anim_class)
            materials = []
            overrides = profile.get_editor_property('material_overrides')
            for index, soft_path in enumerate(overrides):
                # UE Python 可将已加载的软引用解析为 UObject；避免把对象调试文本当路径。
                material = soft_path if isinstance(soft_path, unreal.MaterialInterface) else unreal.load_asset(str(soft_path))
                path = material.get_path_name() if material else str(soft_path)
                if not material:
                    raise RuntimeError('角色材质缺失: ' + path)
                # 与平台预览舞台一致：一个材质覆盖全部槽位，不能拍出与真实角色不同的默认盔甲。
                for slot_index in (range(mesh.get_num_materials()) if len(overrides) == 1 else [index]):
                    mesh.set_material(slot_index, material)
                materials.append(path)
            mesh.prestream_textures(30.0, True)
            hero_root = workspace / 'Game/Plugins/DivineBeasts/ContentPacks/Heroes' / ('DBAHeroPack_' + hero)
            output = output_root if output_root else hero_root / 'SourceArt/UI/Portraits'
            output.mkdir(parents=True, exist_ok=True)
            filename = 'Portrait_' + hero + '.png'
            profile_file = hero_root / 'Content/Characters' / ('DA_Appearance_Zodiac_' + hero + '.uasset')
            records.append({'hero': hero, 'profile': profile_path, 'mesh': skeletal_mesh.get_path_name(),
                            'anim_class': anim_class.get_path_name(),
                            'profile_sha256': hashlib.sha256(profile_file.read_bytes()).hexdigest(),
                            'materials': materials,
                            'effective_materials': [mesh.get_material(i).get_path_name() for i in range(mesh.get_num_materials())],
                            'source': str(output / filename)})
        state['handle'] = unreal.register_slate_post_tick_callback(tick)
        print('CHARACTER_PORTRAIT_CAPTURE_SCHEDULED')
    except Exception:
        for actor in reversed(created):
            subsystem.destroy_actor(actor)
        raise
