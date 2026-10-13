#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5/P6 三张独立审核关卡的真实UE5.8回读：只读，不修改/不保存，验证类型、放置物与权威边界。"""
from __future__ import annotations
import json
import os
from pathlib import Path
import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
BASE="/Game/Development/Foundation/PluginReviews"
MAPS=BASE+"/Maps"
BPS=BASE+"/Blueprints"
SPECS=(
("L_GP_CombatReview","BP_GP_P5ReviewGameMode",["P5_Combat_PlayerStart","P5_Combat_UninitializedCharacter_1","P5_Combat_UninitializedCharacter_2"]),
("L_GP_InteractionAIReview","BP_GP_P5ReviewGameMode",["P5_AI_PlayerStart","P5_NavigationBounds","P5_Interaction_TargetGeometry"]),
("L_DBA_MainArenaReview","GamePlatformArenaGameMode",["P6_Arena_TeamA_Spawn1","P6_Arena_TeamA_Spawn5","P6_Arena_TeamB_Spawn1","P6_Arena_TeamB_Spawn5"]),
)
OUT_DIR=ROOT/"Saved/Validation/PluginP5P6_20261012"

def main():
    assert str(unreal.Paths.get_project_file_path()).replace("\\","/").endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject")
    assert str(unreal.SystemLibrary.get_engine_version()).startswith("5.8.")
    assets=unreal.EditorAssetLibrary
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    game_bp=assets.load_asset(BPS+"/BP_GP_P5ReviewGameMode")
    pawn_bp=assets.load_asset(BPS+"/BP_GP_P5ReviewCharacter")
    assert isinstance(game_bp,unreal.Blueprint) and isinstance(pawn_bp,unreal.Blueprint)
    real_arena=getattr(unreal,"GamePlatformArenaGameMode",None)
    assert real_arena is not None, "Arena C++ native reflection missing"
    arena_default=unreal.get_default_object(real_arena)
    if arena_default.get_editor_property("default_pawn_class") is not None:
        raise RuntimeError("竞技GameMode不能默认出生未验证Pawn")
    # UE5.8对旧World包GC要求严格。一次Editor进程仅载入一张图，不能先load_asset(UWorld)再调用load_level。
    requested=os.environ.get("P5P6_VALIDATE_MAP","")
    selected=[spec for spec in SPECS if spec[0]==requested]
    if len(selected)!=1:
        raise RuntimeError("必须通过P5P6_VALIDATE_MAP准确指定一张审核图，禁止同进程跨地图持有对象")
    report=[]
    for name,expected_mode,labels in selected:
        path=MAPS+"/"+name
        if not assets.does_asset_exist(path):
            raise RuntimeError("审核地图没有真实包："+path)
        if not level.load_level(path):
            raise RuntimeError("独立编辑器重开地图失败："+path)
        current=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        actualmode=current.get_world_settings().get_editor_property("default_game_mode")
        if actualmode is None:
            raise RuntimeError("审核地图未设置GameMode："+path)
        actual_name=str(actualmode.get_name()) if hasattr(actualmode,"get_name") else str(actualmode)
        if expected_mode not in actual_name:
            raise RuntimeError("GameMode不正确："+name+" expected="+expected_mode+" actual="+actual_name)
        found={a.get_actor_label():a for a in actors.get_all_level_actors()}
        missing=[label for label in labels if label not in found]
        if missing:
            raise RuntimeError("审核地图对象缺失："+name+" "+str(missing))
        if name=="L_DBA_MainArenaReview":
            starts=[a for a in found.values() if a.get_actor_label().startswith("P6_Arena_Team") and isinstance(a,unreal.PlayerStart)]
            if len(starts)!=10:
                raise RuntimeError("MainArena应该有10个真实PlayerStart，实际"+str(len(starts)))
            teams=[str(a.get_editor_property("player_start_tag")) for a in starts]
            if sum(t=="TeamA" for t in teams)!=5 or sum(t=="TeamB" for t in teams)!=5:
                raise RuntimeError("竞技出生点未按两队×5布置："+str(teams))
        if name=="L_GP_InteractionAIReview" and not isinstance(found["P5_NavigationBounds"],unreal.NavMeshBoundsVolume):
            raise RuntimeError("AI/Nav地图实际未放置NavMeshBoundsVolume")
        entry={"map":path,"world":current.get_path_name(),"gameMode":actual_name,
               "count":len(found),"requiredLabels":labels,"result":"loaded_and_type_checked",
               "playableGameplay":False,"productionMatch":False}
        print("P5P6_ASSET_RELOAD_OK",name,actual_name,len(found))
        report.append(entry)
    OUT_DIR.mkdir(parents=True,exist_ok=True)
    out=OUT_DIR/("EditorAssetReload_"+requested+".json")
    out.write_text(json.dumps({"schemaVersion":1,"engine":str(unreal.SystemLibrary.get_engine_version()),
        "maps":report,"blueprints":[game_bp.get_path_name(),pawn_bp.get_path_name()],
        "currentGameplayStatus":"review_layout_only_no_server_admission",
        "currentArenaStatus":"waiting_assignment_no_fake_ticket",
        "manualAcceptance":"pending"},ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("P5P6_ASSET_RELOAD_ALL_OK",len(report))
if __name__=="__main__":
    main()
