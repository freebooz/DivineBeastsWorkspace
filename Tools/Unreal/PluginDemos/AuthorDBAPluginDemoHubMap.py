# -*- coding: utf-8 -*-
"""
神兽联盟插件门户开发地图的UE5.8资产作者（非Widget创建器）。

地图属于 /Game/Development/PluginDemos/Maps（正式Cook已排除）。
每次必须由真实 UnrealEditor-Cmd 编辑器操作；已存在则只检查，绝不删除/覆盖别的会话。
不修改 GameDefaultMap，不创建真实登录/联网账号或任何生产地图。
Monolith只负责真正的门户Widget蓝图及样式。
"""
from __future__ import annotations
import json
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[3]
PACKAGE="/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub"
EVIDENCE=ROOT/"Saved/Validation/PluginDemos/Portal"
EVIDENCE.mkdir(parents=True,exist_ok=True)

def ensure_project():
    name=str(unreal.Paths.get_project_file_path()).replace(chr(92),"/")
    if not name.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝修改非神兽联盟主工程：" + name)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("必须使用UE5.8")
    config=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/PluginDemos")' not in config:
        raise RuntimeError("未找到正式Cook排除策略，拒绝创建开发地图")

def spawn(editor,name,klass,pos,rot=None):
    rot=rot or unreal.Rotator(0,0,0)
    a=editor.spawn_actor_from_class(klass,unreal.Vector(*pos),rot)
    if not a:
        raise RuntimeError("无法创建真正的UEActor："+name)
    a.set_actor_label(name)
    return a

def main():
    ensure_project()
    assets=unreal.EditorAssetLibrary
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.does_asset_exist(PACKAGE):
        print("DBA_PORTAL_MAP_ALREADY_EXISTS_NO_OVERWRITE",PACKAGE)
        return
    directory=PACKAGE.rpartition("/")[0]
    if not assets.does_directory_exist(directory):
        if not assets.make_directory(directory):
            raise RuntimeError("开发目录创建失败")
    if not levels.new_level(PACKAGE):
        raise RuntimeError("真实门户地图创建失败")
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError("当前UE编辑器未创建有效UWorld")
    # 通用UE规则类只提供本地PlayerController，登录/演示页面归各自子系统，不拷贝玩法。
    world.get_world_settings().set_editor_property("default_game_mode",unreal.GameModeBase)
    floor=spawn(actors,"DBA_PluginPortal_Floor",unreal.StaticMeshActor,(0,0,-80))
    shape=assets.load_asset("/Engine/BasicShapes/Cube")
    if not isinstance(shape,unreal.StaticMesh):
        raise RuntimeError("引擎内置Cube网格资源缺失")
    fcomp=floor.get_component_by_class(unreal.StaticMeshComponent)
    fcomp.set_static_mesh(shape)
    floor.set_actor_scale3d(unreal.Vector(50,50,1))
    spawn(actors,"DBA_PluginPortal_PlayerStart",unreal.PlayerStart,(0,-750,160),unreal.Rotator(0,90,0))
    light=spawn(actors,"DBA_PluginPortal_Sun",unreal.DirectionalLight,(-1500,0,4500),unreal.Rotator(-55,15,0))
    component=light.get_component_by_class(unreal.DirectionalLightComponent)
    component.set_editor_property("intensity",3.5)
    spawn(actors,"DBA_PluginPortal_SkyAtmosphere",unreal.SkyAtmosphere,(0,0,0))
    spawn(actors,"DBA_PluginPortal_SkyLight",unreal.SkyLight,(0,0,2000))
    spawn(actors,"DBA_PluginPortal_Camera",unreal.CameraActor,(0,-1500,560),unreal.Rotator(-16,90,0))
    if not levels.save_current_level():
        raise RuntimeError("真实门户地图保存失败")
    if not assets.does_asset_exist(PACKAGE):
        raise RuntimeError("保存后地图未登记在UE资产系统")
    record={"status":"created","package":PACKAGE,"engine":str(unreal.SystemLibrary.get_engine_version()),
        "actors":sorted(a.get_actor_label() for a in actors.get_all_level_actors()),
        "portalWidget":"not_created_Monolith_required","manualApproval":"pending"}
    (EVIDENCE/"AuthorPortalMap.json").write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("DBA_PORTAL_MAP_CREATED",PACKAGE,"ACTORS",len(record["actors"]))

if __name__=="__main__":
    main()
