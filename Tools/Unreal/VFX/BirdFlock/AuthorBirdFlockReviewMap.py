#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群专用 UE5.8 人工审核关卡，禁止覆盖/修改正在开发的正式前端、新手村和开放世界地图。

审核地图包含同一个通用蓝图的三种鸟类实例，并通过BirdCountOverride验证数量配置。
不修改服务器权威场景，也不把编辑器人工视觉验收误报为已通过。
"""
from __future__ import annotations
import os

MODE=os.getenv("BIRD_FLOCK_REVIEW_MODE","inspect").lower()
MAP="/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview"
BASE="/DBAPresentationPack_Core/VFX/Environment/BirdFlock"
BLUEPRINT=BASE+"/Blueprints/BP_DBA_BirdFlockAmbient"
PROFILES=BASE+"/Profiles"


def main():
    if MODE=="inspect":
        print("BIRD_REVIEW_MAP_PLAN",MAP,BLUEPRINT)
        return
    if MODE!="apply":
        raise ValueError("只能inspect/apply")
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject") or not str(
        unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("只允许锁定UE5.8神兽联盟主工程")
    assets=unreal.EditorAssetLibrary
    levels=unreal.EditorLevelLibrary
    if assets.does_asset_exist(MAP):
        world=assets.load_asset(MAP)
        if not isinstance(world,unreal.World):
            raise RuntimeError("目标路径存在非审核关卡资产："+MAP)
        print("BIRD_REVIEW_MAP_REUSED_NO_OVERWRITE",MAP)
        return
    bp_cls=assets.load_blueprint_class(BLUEPRINT)
    if bp_cls is None:
        raise RuntimeError("统一鸟群蓝图未生成或未编译")
    profiles={}
    for name in ("Seagull","Swallow","Eagle"):
        profiles[name]=assets.load_asset(PROFILES+"/DA_DBA_BirdFlock_"+name)
        if profiles[name] is None:
            raise RuntimeError("缺少真实鸟群预设："+name)
    parent=MAP.rsplit("/",1)[0]
    if not assets.does_directory_exist(parent):
        assets.make_directory(parent)
    if not levels.new_level(MAP):
        raise RuntimeError("未创建真实鸟群审核关卡")
    def spawn(cls,label,pos,rot=(0,0,0)):
        actor=levels.spawn_actor_from_class(cls,unreal.Vector(*pos),unreal.Rotator(*rot))
        if actor is None:
            raise RuntimeError("关卡Actor创建失败："+label)
        actor.set_actor_label(label)
        return actor
    # 每个鸟类Actor复用一个BP父类，鸟类资源、数量在实例详情面板直接编辑。
    for name,x,count in (("Seagull",-2800.0,32),("Swallow",0.0,72),("Eagle",2800.0,5)):
        actor=spawn(bp_cls,"BirdFlockReview_"+name,(x,0,0))
        actor.set_editor_property("bird_profile",profiles[name])
        actor.set_editor_property("bird_count_override",count)
        actor.set_editor_property("flock_seed",20261011+count)
        actor.set_editor_property("b_show_editor_preview",True)
        actor.rerun_construction_scripts()
        print("BIRD_REVIEW_CONFIGURED",name,count)
    for typ,label,xyz in (
        (unreal.DirectionalLight,"BirdFlockReview_Sun",(0,0,2600)),
        (unreal.SkyLight,"BirdFlockReview_AmbientSky",(0,0,2200)),
        (unreal.SkyAtmosphere,"BirdFlockReview_Atmosphere",(0,0,0)),
        (unreal.PlayerStart,"BirdFlockReview_PlayerStart",(0,-9500,400)),
        (unreal.CameraActor,"BirdFlockReview_Camera",(0,-8800,1500)),
    ):
        spawn(typ,label,xyz)
    if not levels.save_current_level():
        raise RuntimeError("鸟群真实审查地图无法保存")
    if not isinstance(assets.load_asset(MAP),unreal.World):
        raise RuntimeError("鸟群地图回读失败")
    print("BIRD_REVIEW_MAP_SAVED",MAP)
    print("BIRD_REVIEW_MANUAL_VISUAL_TEST_REQUIRED",True)


if __name__=="__main__":
    main()
