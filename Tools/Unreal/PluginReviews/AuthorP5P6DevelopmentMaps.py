#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5/P6 开发期真实地图与GameMode蓝图创建器（只有新的/Foundation/PluginReviews目录）。

严格条件：正式DivineBeastsArena.uproject、UE5.8、显式AUTHOR_MODE=apply。
本脚本不触碰正式Village/FrontEnd/Arena地图，拒绝覆盖已有目标；新地图位于当前
DefaultGame.ini的DirectoriesToNeverCook /Game/Development/Foundation范围。
开发审核Pawn允许引擎本地移动，但没有合法身份/GAS服务端资格时仍禁止伤害和技能。
Arena审核图使用生产AGamePlatformArenaGameMode，未经后端Assignment只能等待，绝不伪造票据。
"""
from __future__ import annotations
import json
import os
from pathlib import Path

import unreal  # type: ignore

ROOT = Path(__file__).resolve().parents[3]
MAPS = "/Game/Development/Foundation/PluginReviews/Maps"
BLUEPRINTS = "/Game/Development/Foundation/PluginReviews/Blueprints"
MAP_NAMES = (
    "L_GP_CombatReview",
    "L_GP_InteractionAIReview",
    "L_DBA_MainArenaReview",
)
ARTIFACT = ROOT / "Saved/Validation/PluginP5P6_20261012"
MODE = os.environ.get("P5P6_AUTHOR_MODE", "inspect").strip().lower()


def guard() -> None:
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("只能写入DivineBeastsWorkspace的正式工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("只能使用锁定UE5.8")
    if MODE not in ("inspect", "apply"):
        raise RuntimeError("必须设置P5P6_AUTHOR_MODE=apply或inspect")
    # 安全排除开发文件，不允许因为审核场景意外进入正式发布包。
    config = (ROOT / "Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' not in config:
        raise RuntimeError("项目未对Foundation/Development明确设置Cook排除，拒绝创建")


def make_blueprint(assets, asset_tools, name, parent, folder):
    path = folder + "/" + name
    if assets.does_asset_exist(path):
        bp = assets.load_asset(path)
        if not isinstance(bp, unreal.Blueprint):
            raise RuntimeError("目标路径已有不同类别资产：" + path)
        print("P5P6_BLUEPRINT_REUSED", path)
        return bp
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    created = asset_tools.create_asset(name, folder, unreal.Blueprint, factory)
    if not isinstance(created, unreal.Blueprint):
        raise RuntimeError("真正蓝图创建失败：" + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(created)
    if not assets.save_loaded_asset(created, only_if_is_dirty=False):
        raise RuntimeError("蓝图保存失败：" + path)
    print("P5P6_BLUEPRINT_SAVED", path)
    return created


def build_review_blueprints(assets, tools):
    if not hasattr(unreal, "DivineBeastsGameplayCharacter"):
        raise RuntimeError("项目可玩角色编译反射缺失，不能使用普通Pawn冒充GAS项目角色")
    if not assets.does_directory_exist(BLUEPRINTS):
        if not assets.make_directory(BLUEPRINTS):
            raise RuntimeError("无法创建开发蓝图目录")
    pawn = make_blueprint(assets, tools, "BP_GP_P5ReviewCharacter",
        unreal.DivineBeastsGameplayCharacter, BLUEPRINTS)
    pawn_path = BLUEPRINTS + "/BP_GP_P5ReviewCharacter"
    pawn_class = assets.load_blueprint_class(pawn_path)
    if pawn_class is None:
        raise RuntimeError("开发角色蓝图生成类未成功加载")
    mesh_path="/DBAContentPack_Common/Mannequins/DBA/Meshes/SK_Mannequin"
    mannequin=assets.load_asset(mesh_path)
    # UE5.8公共包中的UE5/Meshes/SK_Mannequin实际是Skeleton(骨架)而非SkeletalMesh。
    # 使用经过独立编辑器反射验证的DBA/Meshes/SK_Mannequin真实骨骼网格。
    if isinstance(mannequin,unreal.SkeletalMesh):
        cdo=unreal.get_default_object(pawn_class)
        mesh=cdo.get_editor_property("mesh")
        if not isinstance(mesh,unreal.SkeletalMeshComponent):
            raise RuntimeError("开发角色未继承真实ACharacter SkeletalMeshComponent")
        mesh.set_skeletal_mesh_asset(mannequin)
        # 保留项目原生GAS/Combat/Identity，不写入信任身份和技能授权。
        pawn.modify()
        unreal.BlueprintEditorLibrary.compile_blueprint(pawn)
        if not assets.save_loaded_asset(pawn,only_if_is_dirty=False):
            raise RuntimeError("开发Pawn可视骨骼变更未能保存")
        print("P5P6_CHARACTER_VISUAL_MESH_OK",mesh_path)
    else:
        print("P5P6_CHARACTER_VISUAL_MESH_NOT_MOUNTED",mesh_path,
              "开发图可检查模块/准入/出生边界，但角色外观未验收")
    gm = make_blueprint(assets, tools,"BP_GP_P5ReviewGameMode",
         unreal.GameModeBase,BLUEPRINTS)
    gm_path=BLUEPRINTS+"/BP_GP_P5ReviewGameMode"
    gm_class=assets.load_blueprint_class(gm_path)
    if gm_class is None:
        raise RuntimeError("开发本地GameMode蓝图生成类未成功加载")
    defaults=unreal.get_default_object(gm_class)
    defaults.set_editor_property("default_pawn_class",pawn_class)
    gm.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(gm)
    if not assets.save_loaded_asset(gm,only_if_is_dirty=False):
        raise RuntimeError("开发GameMode蓝图未保存")
    print("P5P6_REVIEW_BLUEPRINTS_OK",pawn_path,gm_path)
    return pawn_class,gm_class


def position_tuple(vec):
    return (round(vec.x), round(vec.y), round(vec.z))


def spawn(actors, cls, label, loc, rot=(0, 0, 0)):
    item = actors.spawn_actor_from_class(
        cls,
        unreal.Vector(*loc),
        unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    if item is None:
        raise RuntimeError("未能创建真实审核Actor：" + label)
    item.set_actor_label(label)
    return item


def cube(assets, actors, label, loc, scale=(1, 1, 1)):
    actor=spawn(actors,unreal.StaticMeshActor,label,loc)
    component=actor.get_component_by_class(unreal.StaticMeshComponent)
    loaded=assets.load_asset("/Engine/BasicShapes/Cube")
    if not isinstance(loaded,unreal.StaticMesh):
        raise RuntimeError("引擎内置几何资源缺失")
    component.set_static_mesh(loaded)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def commons(assets,actors,level,name):
    # 所有样例只有独立地板/天空/照明，不引入业务层玩法或伪造服务器事实。
    cube(assets,actors,name+"_Floor",(0,0,-75),(75,65,1.0))
    sun=spawn(actors,unreal.DirectionalLight,name+"_Sun",(-5000,-2000,5000),(-45,35,0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity",4.5)
    spawn(actors,unreal.SkyAtmosphere,name+"_Atmosphere",(0,0,0))
    sky=spawn(actors,unreal.SkyLight,name+"_SkyLight",(0,0,3600))
    sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property("intensity",1.0)
    camera=spawn(actors,unreal.CameraActor,name+"_ReviewCamera",(0,-4300,1800),(-18,90,0))
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(75.0)


def author_level(assets, levels, actors, name, game_mode, build_content):
    path=MAPS+"/"+name
    if assets.does_asset_exist(path):
        obj=assets.load_asset(path)
        if not isinstance(obj,unreal.World):
            raise RuntimeError("审核场景路径已被占用，拒绝替换："+path)
        print("P5P6_MAP_REUSED_NO_OVERWRITE",path)
        return {"name":name,"path":path,"state":"preserved_existing"}
    if not levels.new_level(path):
        raise RuntimeError("UE未创建真实审核地图："+path)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError("新建地图没有编辑器UWorld")
    settings=world.get_world_settings()
    settings.set_editor_property("default_game_mode",game_mode)
    commons(assets,actors,levels,name)
    details=build_content(assets,actors)
    if not levels.save_current_level():
        raise RuntimeError("编辑器保存审核地图失败："+path)
    if not isinstance(assets.load_asset(path),unreal.World):
        raise RuntimeError("保存后重新获取UWorld失败："+path)
    print("P5P6_MAP_SAVED",path,"ACTORS",len(actors.get_all_level_actors()))
    return {"name":name,"path":path,"state":"saved","details":details}


def make_combat(assets,actors,pawn_class):
    pstart=spawn(actors,unreal.PlayerStart,"P5_Combat_PlayerStart",(0,-2350,175),(0,90,0))
    # 两个正式项目角色原型仅提供位置/网格复核，未获得可信Context时GAS/Combat必须FailClosed。
    targets=[]
    for idx,x in enumerate((-650,650)):
        review=spawn(actors,pawn_class,"P5_Combat_UninitializedCharacter_"+str(idx+1),(x,850,130),(0,-90,0))
        targets.append(review.get_actor_label())
    cube(assets,actors,"P5_Combat_ObstacleA",(-1350,100,200),(3,4,4))
    cube(assets,actors,"P5_Combat_ObstacleB",(1350,100,200),(3,4,4))
    return {"characters":targets,"gameplayAuthority":"not_granted_by_editor",
            "playableLocalMovement":"game_mode_default_pawn",
            "damageAndAbilities":"must_require_real_server_admission"}


def make_interaction_ai(assets,actors,pawn_class):
    spawn(actors,unreal.PlayerStart,"P5_AI_PlayerStart",(0,-2100,180),(0,90,0))
    nav=spawn(actors,unreal.NavMeshBoundsVolume,"P5_NavigationBounds",(0,250,0))
    nav.set_actor_scale3d(unreal.Vector(65,56,8))
    for i,x in enumerate((-2400,0,2400)):
        cube(assets,actors,"P5_AI_NavigationObstacle_"+str(i+1),(x,300,200),(3.5,5,4))
    cube(assets,actors,"P5_Interaction_TargetGeometry",(1200,1600,120),(2,2,2))
    # 不提供虚假的交互成功：当前Editor尚未加载Interaction/AI反射模块，后续编译激活后补BP组件。
    for i,x in enumerate((-500,500)):
        spawn(actors,pawn_class,"P5_AI_ReviewCharacter_NeedsRealAIProfile_"+str(i+1),(x,1300,125),(0,-90,0))
    return {"navBounds":nav.get_actor_label(),"interactionContract":"component_not_yet_authored",
            "aiController":"needs_GamePlatformAI_server_module_and_real_Brain_Asset",
            "quest":"needs_QuestDefinition_and_server_progress"}


def make_arena(assets,actors):
    starts=[]
    for side, xpos, yaw in (("TeamA",-2700,0),("TeamB",2700,180)):
        for slot in range(5):
            actor=spawn(actors,unreal.PlayerStart,
                f"P6_Arena_{side}_Spawn{slot+1}",
                (xpos,(slot-2)*700,160),(0,yaw,0))
            # 只是开发布局候选，不能用标签证明项目ProductionMode已被批准。
            actor.set_editor_property("player_start_tag",unreal.Name(side))
            starts.append(actor.get_actor_label())
    cube(assets,actors,"P6_Arena_CenterPlatform",(0,0,-10),(11,9,0.4))
    for i,y in enumerate((-1800,1800)):
        cube(assets,actors,"P6_Arena_Cover_"+str(i+1),(0,y,200),(6,1.5,4))
    return {"spawnPoints":starts,"modeSource":"existing_GamePlatformArenaBuiltinModes",
            "productionMode":"not_configured","verifiedRosterRequired":True,
            "defaultPawnClass":"none_until_trusted_assignment"}


def main():
    guard()
    assets=unreal.EditorAssetLibrary
    paths=[MAPS+"/"+n for n in MAP_NAMES]
    if MODE=="inspect":
        print("P5P6_AUTHOR_PLAN",paths)
        print("P5P6_AUTHOR_INSPECT_OK")
        return
    ARTIFACT.mkdir(parents=True,exist_ok=True)
    for directory in (MAPS,BLUEPRINTS):
        if not assets.does_directory_exist(directory):
            if not assets.make_directory(directory):
                raise RuntimeError("无法创建独立开发目录："+directory)
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    pawn_class,game_mode=build_review_blueprints(assets,tools)
    arena_mode=getattr(unreal,"GamePlatformArenaGameMode",None)
    if arena_mode is None:
        raise RuntimeError("正式ArenaGameMode反射类不存在，绝不能退回GameModeBase冒充竞技")
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    report=[]
    report.append(author_level(assets,levels,actors,MAP_NAMES[0],game_mode,
        lambda assets,actors:make_combat(assets,actors,pawn_class)))
    report.append(author_level(assets,levels,actors,MAP_NAMES[1],game_mode,
        lambda assets,actors:make_interaction_ai(assets,actors,pawn_class)))
    report.append(author_level(assets,levels,actors,MAP_NAMES[2],arena_mode,
        make_arena))
    outcome={"engineVersion":str(unreal.SystemLibrary.get_engine_version()),
        "project":str(unreal.Paths.get_project_file_path()),
        "stage":"development_review_assets_created_not_production_match",
        "maps":report,"productionArenaConfigured":False,
        "aiAndInteractionRuntimeAuthoringBlocked":True,
        "humanAcceptance":"pending"}
    (ARTIFACT/"ReviewMapAuthoring.json").write_text(json.dumps(outcome,ensure_ascii=False,indent=2),encoding="utf-8")
    print("P5P6_AUTHOR_COMPLETED",len(report))


if __name__=="__main__":
    main()
