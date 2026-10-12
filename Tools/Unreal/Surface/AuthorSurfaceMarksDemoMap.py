#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真正的单工程平台表面痕迹演示地图：五块可命中的测试板+专用Actor。
环境SURFACE_MARK_DEMO_MODE=inspect(默认)/author/verify；author仅创建不存在的地图。
不改写项目默认地图、登录流程、全局配置或已有其他插件的演示资产。
"""
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MODE=os.environ.get("SURFACE_MARK_DEMO_MODE","inspect").strip().lower()
MAP="/GamePlatformSurface/Maps/L_GP_SurfaceMarks_Review"
MAP_FILE=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Maps/L_GP_SurfaceMarks_Review.umap"
PANEL_SPACING=360.0
KINDS=("Bullet","Scorch","Slash","Fluid","Crack")

def main():
    if MODE not in ("inspect","author","verify"):raise RuntimeError("模式只接受inspect/author/verify")
    print("SURFACE_MARK_DEMO_TARGET",MAP,MODE,flush=True)
    if MODE=="inspect":return
    import unreal
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("非唯一正式主工程拒绝生成地图")
    script_command=unreal.SystemLibrary.get_command_line().lower()
    if MODE=="author" and ("-unattended" not in script_command or "-scripterrorsarefatal" not in script_command):
        raise RuntimeError("写入需要无人值守、失败致命的完整UE Editor")
    level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if not level or not actors or not editor or level.is_in_play_in_editor():
        raise RuntimeError("须退出PIE并初始化真实编辑器子系统")
    asset=unreal.EditorAssetLibrary
    registry=unreal.AssetRegistryHelpers.get_asset_registry()
    registry.scan_paths_synchronous(["/GamePlatformSurface"],force_rescan=True)
    demo_actor=getattr(unreal,"GamePlatformSurfaceMarkDemoActor",None)
    if demo_actor is None:raise RuntimeError("平台客户端演示Actor尚未真实编译")
    cube=unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    if not isinstance(cube,unreal.StaticMesh):raise RuntimeError("引擎Cube缺失")

    exists=asset.does_asset_exist(MAP) or MAP_FILE.exists()
    if MODE=="author" and not exists:
        if not level.new_level(MAP,is_partitioned_world=False):
            raise RuntimeError("创建地图失败，空包保留供人工检查")
        world=editor.get_editor_world()
        if world is None or world.get_path_name()!=MAP+"."+MAP.rsplit("/",1)[-1]:
            raise RuntimeError("当前编辑世界不是目标地图")
        mode=unreal.load_class(None,"/Script/Engine.GameModeBase")
        world.get_world_settings().set_editor_property("default_game_mode",mode)
        def spawn(cls,label,location,rotation=(0,0,0),scale=(1,1,1)):
            actor=actors.spawn_actor_from_class(cls,unreal.Vector(*location),
                 unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]),transient=False)
            if actor is None:raise RuntimeError("创建Actor失败："+label)
            actor.set_actor_label(label)
            actor.set_actor_scale3d(unreal.Vector(*scale))
            return actor
        ground=spawn(unreal.StaticMeshActor,"MarksReview_Ground",(0,0,-30),scale=(23,15,.4))
        ground.get_editor_property("static_mesh_component").set_static_mesh(cube)
        for index,kind in enumerate(KINDS):
            x=(index-2)*PANEL_SPACING
            panel=spawn(unreal.StaticMeshActor,"MarkPanel_"+kind,(x,300,170),scale=(2.7,.36,3.2))
            cmp=panel.get_editor_property("static_mesh_component")
            if not cmp.set_static_mesh(cube):raise RuntimeError("网格资产无法绑定")
            cmp.set_collision_profile_name("BlockAll")
            cmp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
        spawn(unreal.PlayerStart,"MarksReview_PlayerStart",(0,-1000,180),(0,90,0))
        sunlight=spawn(unreal.DirectionalLight,"MarksReview_Sun",(0,0,1050),(-50,-30,0))
        sunlight.get_component_by_class(unreal.DirectionalLightComponent).set_intensity(3.0)
        demo=spawn(demo_actor,"MarksReview_Emitter",(0,0,0))
        if not level.save_current_level():raise RuntimeError("UE地图保存失败")
        print("SURFACE_MARK_DEMO_SAVED",demo.get_path_name(),flush=True)
    else:
        if not exists:raise RuntimeError("演示地图尚未由UE生成，不伪造验收")
    if not level.load_level(MAP):raise RuntimeError("演示地图回读失败")
    observed=actors.get_all_level_actors()
    names={actor.get_actor_label() for actor in observed}
    required={"MarksReview_Emitter","MarksReview_Ground","MarksReview_PlayerStart","MarksReview_Sun"}
    required.update("MarkPanel_"+k for k in KINDS)
    if not required.issubset(names):
        raise RuntimeError("演示地图必需Actor缺失："+str(sorted(required-names)))
    print("SURFACE_MARK_DEMO_UE_MAP_VERIFIED",len(required),str(MAP_FILE),flush=True)

if __name__=="__main__":main()
