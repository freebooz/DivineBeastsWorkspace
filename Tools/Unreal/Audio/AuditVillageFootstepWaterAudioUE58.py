# -*- coding: utf-8 -*-
"""UE5.8新手村脚步/水声原生资产只读审计。

仅检查已加载关卡Actor和正式动画；不创建Actor、不修改动画、也不保存地图。
未加载的World Partition分区、真实音频输出和性能必须单独验收。
"""
import os
import unreal

VILLAGE_MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"
FORMAL_ANIMATIONS = (
    "/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Walk_Fwd",
    "/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MF_Unarmed_Jog_Fwd",
)
ALLOWED_LOOPS = {"Ambience.WaterStreamA", "Ambience.WaterStreamB"}
REQUIRE_SCENE = os.environ.get("SSLM_REQUIRE_VILLAGE_AUDIO_ANCHORS", "0") == "1"
REQUIRE_ANIMS = os.environ.get("SSLM_REQUIRE_FORMAL_FOOTSTEP_NOTIFY", "0") == "1"


def audit():
    anchor_class = getattr(unreal, "DivineBeastsWaterAudioAnchorComponent", None)
    notify_class = getattr(unreal, "DivineBeastsAudioFootstepNotify", None)
    if anchor_class is None or notify_class is None:
        raise RuntimeError("客户端C++模块未编译/加载，缺新水声组件或脚步Notify反射类")

    world = unreal.EditorLoadingAndSavingUtils.load_map(VILLAGE_MAP)
    if not world:
        raise RuntimeError("无法加载真实新手村关卡：" + VILLAGE_MAP)
    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    anchors = []
    for actor in actors:
        if not actor:
            continue
        for component in actor.get_components_by_class(anchor_class):
            if not actor.actor_has_tag(unreal.Name("SSLM.WaterSource")):
                raise RuntimeError("水声组件Owner不是已标记真实水体Actor：" + actor.get_path_name())
            clip = str(component.get_editor_property("water_loop_clip_id"))
            if clip not in ALLOWED_LOOPS:
                raise RuntimeError("水声组件Clip不是两条已验证循环音之一：" + clip)
            point = component.get_world_location()
            if not all(abs(float(v)) < 1.0e12 for v in (point.x, point.y, point.z)):
                raise RuntimeError("水声组件原生坐标无效：" + actor.get_path_name())
            anchors.append((actor.get_path_name(), component.get_path_name(), clip))
            # 此处打印的是UE原生Actor/Component实际位置，不向地图写入任何猜测坐标。
            print("SSLM_VILLAGE_AUDIO_REAL_ANCHOR", actor.get_path_name(),
                  component.get_path_name(), clip, point)
    print("SSLM_VILLAGE_AUDIO_LOADED_ACTORS", len(actors))
    print("SSLM_VILLAGE_AUDIO_REAL_ANCHORS", len(anchors))
    if not anchors:
        print("SSLM_VILLAGE_AUDIO_PENDING_NO_CONFIRMED_SCENE_SOURCE")
        if REQUIRE_SCENE:
            raise RuntimeError("正式新手村当前加载的场景尚无可验证的水声组件")

    library = unreal.AnimationLibrary
    ready = 0
    for path in FORMAL_ANIMATIONS:
        anim = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(anim, unreal.AnimSequence):
            raise RuntimeError("无法加载正式脚步动画：" + path)
        found = []
        for track in library.get_animation_notify_track_names(anim):
            for event in library.get_animation_notify_events_for_track(anim, track):
                notify = event.get_editor_property("notify")
                if isinstance(notify, notify_class):
                    found.append(notify)
                    print("SSLM_VILLAGE_FOOTSTEP_NATIVE_NOTIFY", path,
                          str(notify.get_editor_property("foot_socket_name")),
                          str(notify.get_editor_property("default_clip_id")))
        if len(found) >= 2:
            ready += 1
        else:
            print("SSLM_VILLAGE_FOOTSTEP_FORMAL_PENDING", path, len(found))
    print("SSLM_VILLAGE_FOOTSTEP_FORMAL_READY", ready, len(FORMAL_ANIMATIONS))
    if REQUIRE_ANIMS and ready != len(FORMAL_ANIMATIONS):
        raise RuntimeError("正式行走/慢跑动画尚未有足够的脚步Notify")
    print("SSLM_VILLAGE_AUDIO_NATIVE_AUDIT_OK", len(anchors), ready,
          "read_only=1,not_a_listening_or_performance_test=1")


if __name__ == "__main__":
    audit()
