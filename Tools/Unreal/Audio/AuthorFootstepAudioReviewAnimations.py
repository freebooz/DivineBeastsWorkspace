# -*- coding: utf-8 -*-
"""SSLM脚步动画音频：只生成两份独立审核副本，不修改正式Locomotion。
审核序列中通知时间为预估审核点，实际脚掌触地时间必须在UE动画编辑器人工定位后
才能正式迁入原Animation Sequence，避免错误频率或打击时机。
"""
import os
import unreal

SOURCE = "/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion"
DEST = "/Game/Development/Audio/Animations"
TRACK = "SSLM_AudioReview"
MODE = os.environ.get("SSLM_FOOTSTEP_REVIEW_MODE","inspect").strip().lower()
ROWS = (
    ("MF_Unarmed_Walk_Fwd","AS_SSLM_AudioReview_Walk",0.22,0.70),
    ("MF_Unarmed_Jog_Fwd","AS_SSLM_AudioReview_Jog",0.20,0.68),
)

def main():
    lib=unreal.EditorAssetLibrary
    unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
        ["/DBAContentPack_Common","/Game/Development/Audio"],True)
    kind=getattr(unreal,"DivineBeastsAudioFootstepNotify",None)
    if kind is None:
        raise RuntimeError("UE编辑器模块尚未包含已编译的UDivineBeastsAudioFootstepNotify")
    library=unreal.AnimationLibrary
    print("SSLM_FOOTSTEP_REVIEW_CLASS_OK",str(kind))
    for original,review,part_left,part_right in ROWS:
        source=SOURCE+"/"+original
        target=DEST+"/"+review
        sequence=lib.load_asset(source)
        if not isinstance(sequence,unreal.AnimSequence):
            raise RuntimeError("动画源无法由Editor真实读取："+source)
        existing=lib.does_asset_exist(target)
        print("SSLM_FOOTSTEP_REVIEW_PLAN",original,review,sequence.get_play_length(),existing)
        if MODE=="inspect":
            continue
        if existing:
            raise FileExistsError("已存在脚步审核副本，拒绝覆盖："+target)
        if not lib.does_directory_exist(DEST):
            lib.make_directory(DEST)
        if not lib.duplicate_asset(source,target):
            raise RuntimeError("UE无法复制原生动画资产："+source)
        copied=lib.load_asset(target)
        if not isinstance(copied,unreal.AnimSequence):
            raise RuntimeError("UE生成的音频审核动画不是AnimSequence："+target)
        duration=float(copied.get_play_length())
        if duration<=0:
            raise RuntimeError("审核动画时长无效："+target)
        library.add_animation_notify_track(copied,unreal.Name(TRACK),unreal.LinearColor(0.05,0.7,0.95,1.0))
        for frac,foot,clip in (
            (part_left,"foot_l","Footstep.LeavesA"),
            (part_right,"foot_r","Footstep.LeavesB")
        ):
            when=duration*frac
            notify=library.add_animation_notify_event(copied,unreal.Name(TRACK),float(when),kind)
            if not isinstance(notify,kind):
                raise RuntimeError("UE未能在审核动画添加正式C++ Notify类："+target)
            notify.set_editor_property("foot_socket_name",unreal.Name(foot))
            notify.set_editor_property("default_clip_id",unreal.Name(clip))
            print("SSLM_FOOTSTEP_REVIEW_NOTIFY_AUTHORED",review,foot,clip,when)
        if not lib.save_loaded_asset(copied,only_if_is_dirty=False):
            raise RuntimeError("UE无法保存带通知的审核动画："+target)
        print("SSLM_FOOTSTEP_REVIEW_ANIM_SAVED",target)
    print("SSLM_FOOTSTEP_REVIEW_DONE",len(ROWS),MODE)

if __name__ == "__main__":
    if MODE not in ("inspect","apply"):
        raise ValueError("SSLM_FOOTSTEP_REVIEW_MODE只接受inspect/apply")
    main()
