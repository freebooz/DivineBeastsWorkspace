# -*- coding: utf-8 -*-
"""预览工作室光照与构图更新，仅由锁定UE编辑器/Monolith执行。
要求当前打开既定工作室地图，无未保存的其他用户资产。保留既有舞台身份，
仅旧室内工作室更新三点光；圣境场景复用唯一月光作者规则，避免旧工具重跑恢复日光。
增加无碰撞背景/地面并固定曝光；只保存此地图。
不进入正式服务器世界，不改登录流程、不创建用户界面或权威碰撞。
"""
import json
import runpy
from pathlib import Path
import unreal

PATH = '/DBAFrontEndPack/Maps/L_DBA_CharacterStudio'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == PATH, '先通过Monolith打开既定预览工作室'
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in actors_api.get_all_level_actors()}
has_sanctuary = any(item.actor_has_tag('DBA.Preview.ZodiacSanctuary') for item in actors.values())

def actor(label, cls, position, scale=None):
    """复用命名Actor，不重复生成；新增对象只归当前工作室关卡。"""
    item = actors.get(label)
    if not item:
        item = actors_api.spawn_actor_from_class(cls, unreal.Vector(*position))
        assert item, '生成工作室Actor失败: ' + label
        item.set_actor_label(label)
        actors[label] = item
    item.set_actor_location(unreal.Vector(*position), False, False)
    if scale:
        item.set_actor_scale3d(unreal.Vector(*scale))
    return item

for label, position, lumens in (() if has_sanctuary else (
    ('PreviewKeyLight', (220, 120, 230), 3000),
    ('PreviewFillLight', (120, -170, 140), 1800),
    ('PreviewRimLight', (-120, 80, 230), 2200),
)):
    item = actor(label, unreal.PointLight, position)
    component = item.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    component.set_editor_property('intensity', float(lumens))
    component.set_editor_property('attenuation_radius', 900.0)

# 2026-10-11真实关卡采用Landscape（地形）。重跑旧工作室调整脚本不得重新放置
# PreviewFloor和PreviewBackdrop，否则会遮挡已制作的地形与落地花瓣。
has_natural_landscape = any(
    item.get_class().get_name() == 'Landscape' for item in actors.values()
)
if has_natural_landscape:
    print('PRESERVE_LANDSCAPE_SKIP_LEGACY_PREVIEW_FLOOR_AND_BACKDROP')
else:
    material = unreal.load_asset('/DBAFrontEndPack/Materials/M_DBA_PreviewBackdrop')
    assert material, '工作室背景材质尚未由Monolith保存'
    for label, mesh_path, location, scale in (
        ('PreviewFloor', '/Engine/BasicShapes/Plane', (0, 0, -1), (12, 12, 1)),
        ('PreviewBackdrop', '/Engine/BasicShapes/Cube', (-150, 0, 145), (0.1, 12, 8)),
    ):
        item = actor(label, unreal.StaticMeshActor, location, scale)
        component = item.get_component_by_class(unreal.StaticMeshComponent)
        component.set_editor_property('static_mesh', unreal.load_asset(mesh_path))
        component.set_material(0, material)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

volume = actor('PreviewExposure', unreal.PostProcessVolume, (0, 0, 0))
volume.set_editor_property('unbound', True)
settings = volume.get_editor_property('settings')
# 当前项目关闭扩展亮度范围，Min/Max使用线性亮度而非EV100。
# 室内三点灯仅数千流明，线性亮度16使材质严重欠曝；固定1并保持零补偿。
settings.set_editor_property('override_auto_exposure_min_brightness', True)
settings.set_editor_property('override_auto_exposure_max_brightness', True)
assert unreal.SystemLibrary.get_console_variable_int_value('r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange') == 0, '曝光单位已改变，须重新校准而非套用线性数值'
settings.set_editor_property('auto_exposure_min_brightness', 1.0)
settings.set_editor_property('auto_exposure_max_brightness', 1.0)
settings.set_editor_property('override_auto_exposure_bias', True)
settings.set_editor_property('auto_exposure_bias', 0.0)
volume.set_editor_property('settings', settings)

# 圣境与旧室内工作室不能共用曝光强度：仅调用作者文件的明确光照函数，
# 不导入/重建场景、不重设舞台和用户手工Actor。真实执行仍须由Monolith触发。
if has_sanctuary:
    authoring = runpy.run_path(str(Path(__file__).with_name('ImportZodiacSanctuary.py')))
    authoring['configure_studio_moonlight'](actors)

stage = actors['CharacterPreviewStage']
pivot = next(c for c in stage.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'CameraPivot')
# 用户指定的正面完整角色构图；环境修复重跑时同样保留该镜头，不恢复高处广角远景。
# 角色网格比例、鼠标旋转和待机动画沿用平台舞台，不参与服务器世界控制。
pivot.set_editor_property('relative_location', unreal.Vector(0, 0, 95))
camera = stage.get_component_by_class(unreal.CameraComponent)
stage.set_camera_distance(430.)
camera.set_editor_property('relative_rotation', unreal.Rotator(pitch=0, yaw=180, roll=0))
camera.set_editor_property('field_of_view', 55.)
camera.set_editor_property('aspect_ratio', 16./9.)
camera.set_editor_property('override_aspect_ratio_axis_constraint', True)
camera.set_editor_property('aspect_ratio_axis_constraint', unreal.AspectRatioAxisConstraint.ASPECT_RATIO_MAINTAIN_YFOV)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), '保存工作室失败'
print(json.dumps({'map': PATH, 'lighting': 'Moonlight' if has_sanctuary else 'LegacyIndoor', 'fixedExposureLinearBrightness': .4 if has_sanctuary else 1, 'actors': list(actors.keys())}, ensure_ascii=False))
