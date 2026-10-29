# -*- coding: utf-8 -*-
"""SSLM UE5.8：只允许在独立水体审核地图配置 WaterBodyLake。

在 UE5.8 编辑器的 Python 控制台执行；默认 APPLY=False 只读。
先人工审核地图、保存其他未提交的编辑器更改，再设置 APPLY=True。
本脚本不伪造 .uasset/.umap，不编辑正式新手村，不创建无 Brush 的 PhysicsVolume。
"""
import unreal

APPLY = False
EXPECTED_MAP = "/Game/Development/Water/L_GP_WaterReview"
LAKE_MATERIAL = "/GamePlatformSurface/Materials/Water/MI_GP_Lake_Native"
UNDERWATER_MATERIAL = "/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume"


def note(message):
    unreal.log("[SSLM.WaterReview] " + str(message))


def require_review_map():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError("当前没有可读的UE编辑器世界")
    package = str(world.get_outermost().get_name())
    if package != EXPECTED_MAP:
        raise RuntimeError("只能操作独立审核地图，实际地图=" + package)
    if APPLY:
        dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if dirty:
            raise RuntimeError("当前仍有脏地图，先人工审核、保存：" +
                               ", ".join(str(item.get_name()) for item in dirty))
    return world


def get_component(lake):
    component = lake.get_water_body_component()
    if not component:
        raise RuntimeError("WaterBodyLake不存在WaterBodyComponent，拒绝修改")
    return component


def protect_landscape(actor, component):
    # 在Actor或Component上查找当前UE5.8反射字段，发现自动雕刻就关闭。
    for obj in (component, actor):
        try:
            setting = obj.get_editor_property("affects_landscape")
        except Exception:
            continue
        if setting:
            obj.set_editor_property("affects_landscape", False)
            note("已在独立审核地图关闭AffectsLandscape")
        return
    raise RuntimeError("无法安全访问affects_landscape：需在Details中人工关闭")


def main():
    require_review_map()
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    existing = list(actors.get_all_level_actors())
    lakes = [a for a in existing if isinstance(a, unreal.WaterBodyLake)]
    zones = [a for a in existing if a.get_class().get_name() == "WaterZone"]
    volumes = [a for a in existing if isinstance(a, unreal.PhysicsVolume)
               and bool(a.get_editor_property("water_volume"))]
    note("审核地图当前：Lake={} Zone={} SwimmingVolume={} APPLY={}".format(
        len(lakes), len(zones), len(volumes), APPLY))
    if len(lakes) > 1:
        raise RuntimeError("存在多个WaterBodyLake；不能猜测应该修改哪个")
    if not APPLY:
        if lakes:
            component = get_component(lakes[0])
            note("水面材质={}；水下材质={}".format(
                component.get_editor_property("water_material"),
                component.get_editor_property("underwater_post_process_material")))
        note("只读预检完成；人工确认后才把APPLY置True")
        return

    water_surface = unreal.load_asset(LAKE_MATERIAL)
    underwater = unreal.load_asset(UNDERWATER_MATERIAL)
    if not water_surface or not underwater:
        raise RuntimeError("无法加载水面材质或水下后处理材质；没有写入地图")

    # 仅在审核地图没有湖泊时创建官方原生湖泊Actor。
    if lakes:
        lake = lakes[0]
    else:
        lake = actors.spawn_actor_from_class(unreal.WaterBodyLake, unreal.Vector(0, 0, 0))
        if not lake:
            raise RuntimeError("编辑器创建WaterBodyLake失败")
        lake.set_actor_label("Review_Lake")
        note("已新建审核湖泊；仍需人工调整Spline为真实湖岸")

    component = get_component(lake)
    protect_landscape(lake, component)
    if not component.get_editor_property("water_material"):
        component.set_water_material(water_surface)
    if not component.get_editor_property("underwater_post_process_material"):
        component.set_underwater_post_process_material(underwater)
    if not component.get_editor_property("generate_overlap_events"):
        component.set_editor_property("generate_overlap_events", True)

    settings = component.get_editor_property("underwater_post_process_settings")
    settings.set_editor_property("enabled", True)
    settings.set_editor_property("blend_weight", 1.0)
    settings.set_editor_property("priority", 1.0)
    settings.set_editor_property("blend_radius", 100.0)
    component.set_editor_property("underwater_post_process_settings", settings)
    component.on_water_body_changed(False, False, True)

    if not unreal.EditorLoadingAndSavingUtils.save_current_level():
        raise RuntimeError("审核地图保存失败，不能视为完成")
    note("审核地图保存成功；后续须UE冷重载、进入水下和多人测试")
    if not zones:
        note("警告：未预先检测到WaterZone；检查引擎是否自动建立及WaterMesh覆盖")
    if not volumes:
        note("警告：没有游泳PhysicsVolume；需要编辑器手动制作有效Brush并启用Water Volume")


if __name__ == "__main__":
    main()
