# -*- coding: utf-8 -*-
"""SSLM脚步审核动画独立UE冷回读：验证每份真实AnimSequence包含两次C++脚步通知。
不修改正式WorldLocomotion原始动画，审核时间点仍须人工视觉确认。
"""
import unreal
base="/Game/Development/Audio/Animations"
origin="/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion"
track=unreal.Name("SSLM_AudioReview")
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
    [base,origin], True)
names=["AS_SSLM_AudioReview_Walk","AS_SSLM_AudioReview_Jog"]
lib=unreal.AnimationLibrary
for name in names:
    sequence=unreal.EditorAssetLibrary.load_asset(base+"/"+name)
    if not isinstance(sequence,unreal.AnimSequence):
        raise RuntimeError("冷重载动画资产不是原生AnimSequence："+name)
    if track not in lib.get_animation_notify_track_names(sequence):
        raise RuntimeError("审核动画缺脚步轨道："+name)
    events=lib.get_animation_notify_events_for_track(sequence,track)
    if len(events)!=2:
        raise RuntimeError("审核动画C++脚步通知数量异常："+name+" count="+str(len(events)))
    for i,event in enumerate(events):
        notify=event.get_editor_property("notify")
        if not isinstance(notify,unreal.DivineBeastsAudioFootstepNotify):
            raise RuntimeError("C++音效通知类身份错误："+name+" event="+str(i))
        print("SSLM_FOOTSTEP_REVIEW_COLD_NOTIFY",name,
              str(notify.get_editor_property("foot_socket_name")),
              str(notify.get_editor_property("default_clip_id")))
    print("SSLM_FOOTSTEP_REVIEW_COLD_ANIM_OK",name,len(events))
for src in ["MF_Unarmed_Walk_Fwd","MF_Unarmed_Jog_Fwd"]:
    formal=unreal.EditorAssetLibrary.load_asset(origin+"/"+src)
    if not isinstance(formal,unreal.AnimSequence) or track in lib.get_animation_notify_track_names(formal):
        raise RuntimeError("正式动画被错误修改或类型不正确："+src)
    print("SSLM_FOOTSTEP_FORMAL_ANIM_UNCHANGED",src)
print("SSLM_FOOTSTEP_REVIEW_COLD_VERIFY_OK",len(names),4)
