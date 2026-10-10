# -*- coding: utf-8 -*-
"""预览工作室光照与构图更新，仅由锁定UE编辑器/Monolith执行。
要求当前打开既定工作室地图，无未保存的其他用户资产。保留既有舞台身份，
更新三点动态光，增加无碰撞背景/地面并固定曝光；只保存此地图。
不进入正式服务器世界，不改登录流程、不创建用户界面或权威碰撞。
"""
import json
import unreal

PATH = '/DBAFrontEndPack/Maps/L_DBA_CharacterStudio'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_path_name().split('.')[0] == PATH, '先通过Monolith打开既定预览工作室'
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in actors_api.get_all_level_actors()}

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

for label, position, lumens in (
    ('PreviewKeyLight', (220, 120, 230), 3000),
    ('PreviewFillLight', (120, -170, 140), 1800),
    ('PreviewRimLight', (-120, 80, 230), 2200),
):
    item = actor(label, unreal.PointLight, position)
    component = item.get_component_by_class(unreal.PointLightComponent)
    component.set_mobility(unreal.ComponentMobility.MOVABLE)
    component.set_editor_property('intensity_units', unreal.LightUnits.LUMENS)
    component.set_editor_property('intensity', float(lumens))
    component.set_editor_property('attenuation_radius', 900.0)

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

stage = actors['CharacterPreviewStage']
pivot = next(c for c in stage.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'CameraPivot')
# 现代角色页把主体放在中央，两侧固定宽度面板展示选项；高度仍为平台默认95cm。
pivot.set_editor_property('relative_location', unreal.Vector(0, 0, 95))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), '保存工作室失败'
print(json.dumps({'map': PATH, 'lightUnits': 'Lumens', 'fixedExposureLinearBrightness': 1, 'actors': list(actors.keys())}, ensure_ascii=False))
