# -*- coding: utf-8 -*-
"""
神兽联盟前端三维场景地图生成器。
只由UE5.8 Editor执行，真实创建.umap；不在运行时执行。
"""

import unreal

FRONT_END_MAP = "/DBAFrontEndPack/Maps/L_DBA_FrontEnd"
CHARACTER_STUDIO_MAP = "/DBAFrontEndPack/Maps/L_DBA_CharacterStudio"


def log(message):
    unreal.log("[DBA FrontEnd] " + str(message))


def require(condition, message):
    if not condition:
        raise RuntimeError("[DBA FrontEnd] " + message)


def new_level(package_path):
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        unreal.EditorAssetLibrary.delete_asset(package_path)
    require(
        unreal.EditorLevelLibrary.new_level(package_path),
        "创建地图失败: " + package_path,
    )


def spawn(actor_class, location, rotation, label):
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        actor_class,
        location,
        rotation,
    )
    require(actor is not None, "生成Actor失败: " + label)
    actor.set_actor_label(label)
    return actor


def create_front_end_map():
    log("创建客户端前端宿主地图。")
    new_level(FRONT_END_MAP)

    # 登录阶段主要由UMG覆盖；这里保留一个稳定本地相机，避免依赖正式游戏世界。
    camera = spawn(
        unreal.CameraActor,
        unreal.Vector(0.0, 0.0, 120.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "FrontEndCamera",
    )

    require(
        unreal.EditorLevelLibrary.save_current_level(),
        "保存前端地图失败。",
    )


def create_character_studio_map():
    log("创建角色三维预览工作室地图。")
    new_level(CHARACTER_STUDIO_MAP)

    stage_class = getattr(unreal, "GamePlatformCharacterPreviewStage", None)
    require(stage_class is not None, "未找到AGamePlatformCharacterPreviewStage反射类型。")
    spawn(
        stage_class,
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "CharacterPreviewStage",
    )

    # 三点式基础灯光仅用于开发期保证Manny/Quinn和后续正式角色可读；
    # 场景美术后续可以替换，但不改变PreviewStage和地图逻辑身份。
    key = spawn(
        unreal.PointLight,
        unreal.Vector(160.0, 120.0, 260.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "PreviewKeyLight",
    )
    fill = spawn(
        unreal.PointLight,
        unreal.Vector(80.0, -180.0, 170.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "PreviewFillLight",
    )
    rim = spawn(
        unreal.PointLight,
        unreal.Vector(-150.0, 60.0, 230.0),
        unreal.Rotator(0.0, 0.0, 0.0),
        "PreviewRimLight",
    )

    for actor, intensity in ((key, 4500.0), (fill, 2200.0), (rim, 3200.0)):
        component = actor.get_component_by_class(unreal.PointLightComponent)
        if component:
            component.set_editor_property("intensity", intensity)
            component.set_editor_property("attenuation_radius", 900.0)

    require(
        unreal.EditorLevelLibrary.save_current_level(),
        "保存角色工作室地图失败。",
    )


def main():
    create_front_end_map()
    create_character_studio_map()

    require(
        unreal.EditorAssetLibrary.does_asset_exist(FRONT_END_MAP),
        "前端地图保存后未在AssetRegistry中出现。",
    )
    require(
        unreal.EditorAssetLibrary.does_asset_exist(CHARACTER_STUDIO_MAP),
        "角色工作室地图保存后未在AssetRegistry中出现。",
    )
    log("L_DBA_FrontEnd 与 L_DBA_CharacterStudio 生成完成。")


main()
