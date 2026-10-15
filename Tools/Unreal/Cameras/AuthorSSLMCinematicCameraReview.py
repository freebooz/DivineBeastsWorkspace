# -*- coding: utf-8 -*-
"""SSLM电影镜头蓝图与演示地图作者：只创建独立开发资源，禁止覆盖正式关卡。"""
from pathlib import Path
import json
import unreal
ROOT=Path(__file__).resolve().parents[3]
MAP="/Game/Development/PluginDemos/Maps/L_DBA_CinematicCameraReview"
BP="/Game/Development/PluginDemos/Blueprints/BP_DBA_CinematicCameraRig"
MANNY="/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple"
VIEWS=[("DBA_Cine_24mm",24.0,5.6,(-640,-20,260)),
       ("DBA_Cine_35mm",35.0,5.6,(-850,-200,230)),
       ("DBA_Cine_50mm",50.0,2.8,(-1080,0,225)),
       ("DBA_Cine_85mm",85.0,2.8,(-1650,180,215))]
def check_environment():
    project=str(unreal.Paths.get_project_file_path()).replace(chr(92),"/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"): raise RuntimeError("工程身份不匹配: "+project)
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."): raise RuntimeError("需要UE5.8")
    ini=(ROOT/"Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    if 'DirectoriesToNeverCook=(Path="/Game/Development/PluginDemos")' not in ini: raise RuntimeError("演示地图未排除正式Cook")
def spawn(actors,label,cls,pos,rot=None):
    obj=actors.spawn_actor_from_class(cls,unreal.Vector(*pos),rot or unreal.Rotator(0,0,0))
    if not obj: raise RuntimeError("生成Actor失败: "+label)
    obj.set_actor_label(label)
    return obj
def settings(actor,focal,aperture):
    comp=actor.get_cine_camera_component()
    if not comp: raise RuntimeError("缺少CineCameraComponent")
    film=comp.get_editor_property("filmback")
    film.sensor_width=36.0
    comp.set_editor_property("filmback",film)
    comp.set_editor_property("current_focal_length",focal)
    comp.set_editor_property("current_aperture",aperture)
    focus=comp.get_editor_property("focus_settings")
    focus.focus_method=unreal.CameraFocusMethod.MANUAL
    focus.manual_focus_distance=380.0
    comp.set_editor_property("focus_settings",focus)
    if abs(float(comp.get_editor_property("current_focal_length"))-focal)>0.05:
        raise RuntimeError("焦距回读错误")
def main():
    check_environment()
    assets=unreal.EditorAssetLibrary
    asset_tools=unreal.AssetToolsHelpers.get_asset_tools()
    if assets.does_asset_exist(BP):
        bp=assets.load_asset(BP)
        if not isinstance(bp,unreal.Blueprint): raise RuntimeError("已有蓝图类型不匹配")
        bp_state="existing"
    else:
        dir,name=BP.rsplit("/",1)
        if not assets.does_directory_exist(dir): assets.make_directory(dir)
        factory=unreal.BlueprintFactory()
        factory.set_editor_property("parent_class",unreal.CineCameraActor)
        bp=asset_tools.create_asset(name,dir,unreal.Blueprint,factory)
        if not bp: raise RuntimeError("创建真实摄像机蓝图失败")
        if hasattr(unreal,"BlueprintEditorLibrary"):
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if not assets.save_loaded_asset(bp,only_if_is_dirty=False): raise RuntimeError("摄像机蓝图保存失败")
        bp_state="created"
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.does_asset_exist(MAP):
        if not levels.load_level(MAP): raise RuntimeError("既有电影镜头地图无法载入")
        # 只修复本脚本上次中断的独立镜头开发地图；不涉及正式场景。
        present={a.get_actor_label():a for a in actors.get_all_level_actors()}
        changed=False
        cube=assets.load_asset("/Engine/BasicShapes/Cube")
        if "DBA_CameraReview_Floor" not in present:
            floor=spawn(actors,"DBA_CameraReview_Floor",unreal.StaticMeshActor,(0,0,-70))
            floor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
            floor.set_actor_scale3d(unreal.Vector(21,21,1))
            changed=True
        if "DBA_CameraReview_Start" not in present:
            spawn(actors,"DBA_CameraReview_Start",unreal.PlayerStart,(-200,0,160)); changed=True
        if "DBA_CameraReview_Sun" not in present:
            spawn(actors,"DBA_CameraReview_Sun",unreal.DirectionalLight,(-900,-900,1500),unreal.Rotator(-42,15,0)); changed=True
        if "DBA_CameraReview_Sky" not in present:
            spawn(actors,"DBA_CameraReview_Sky",unreal.SkyAtmosphere,(0,0,0)); changed=True
        if "DBA_CameraReview_SkyLight" not in present:
            spawn(actors,"DBA_CameraReview_SkyLight",unreal.SkyLight,(0,0,2000)); changed=True
        if "DBA_CameraReview_Manny" not in present:
            mesh=assets.load_asset(MANNY)
            if not isinstance(mesh,unreal.SkeletalMesh): raise RuntimeError("正式Manny网格未找到")
            actor=spawn(actors,"DBA_CameraReview_Manny",unreal.SkeletalMeshActor,(0,0,0))
            actor.get_component_by_class(unreal.SkeletalMeshComponent).set_skeletal_mesh(mesh)
            changed=True
        for i,x in enumerate((-220,220)):
            label="DBA_CameraReview_Pillar_"+str(i)
            if label not in present:
                pillar=spawn(actors,label,unreal.StaticMeshActor,(100,x,80))
                pillar.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
                pillar.set_actor_scale3d(unreal.Vector(1.5,1.5,3))
                changed=True
        for label,focal,aperture,pos in VIEWS:
            if label not in present:
                import math
                dx,dy,dz=-pos[0],-pos[1],110-pos[2]
                rotation=unreal.Rotator(math.degrees(math.atan2(dz,math.hypot(dx,dy))),math.degrees(math.atan2(dy,dx)),0)
                settings(spawn(actors,label,unreal.CineCameraActor,pos,rotation),focal,aperture)
                changed=True
        if changed and not levels.save_current_level(): raise RuntimeError("补齐镜头地图后保存失败")
        map_state="existing_repaired" if changed else "existing_no_overwrite"
    else:
        d=MAP.rsplit("/",1)[0]
        if not assets.does_directory_exist(d): assets.make_directory(d)
        if not levels.new_level(MAP): raise RuntimeError("创建真实电影镜头地图失败")
        world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode",unreal.GameModeBase)
        cube=assets.load_asset("/Engine/BasicShapes/Cube")
        floor=spawn(actors,"DBA_CameraReview_Floor",unreal.StaticMeshActor,(0,0,-70))
        floor.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
        floor.set_actor_scale3d(unreal.Vector(21,21,1))
        spawn(actors,"DBA_CameraReview_Start",unreal.PlayerStart,(-200,0,160))
        spawn(actors,"DBA_CameraReview_Sun",unreal.DirectionalLight,(-900,-900,1500),unreal.Rotator(-42,15,0))
        spawn(actors,"DBA_CameraReview_Sky",unreal.SkyAtmosphere,(0,0,0))
        spawn(actors,"DBA_CameraReview_SkyLight",unreal.SkyLight,(0,0,2000))
        mesh=assets.load_asset(MANNY)
        if not isinstance(mesh,unreal.SkeletalMesh): raise RuntimeError("正式Manny网格未找到")
        actor=spawn(actors,"DBA_CameraReview_Manny",unreal.SkeletalMeshActor,(0,0,0))
        actor.get_component_by_class(unreal.SkeletalMeshComponent).set_skeletal_mesh(mesh)
        for i,x in enumerate((-220,220)):
            pillar=spawn(actors,"DBA_CameraReview_Pillar_"+str(i),unreal.StaticMeshActor,(100,x,80))
            pillar.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(cube)
            pillar.set_actor_scale3d(unreal.Vector(1.5,1.5,3))
        for label,focal,aperture,pos in VIEWS:
            # 纯数学构图避免依赖某些UE5.8编辑器发行版未暴露的KismetMathLibrary。
            import math
            dx,dy,dz=-pos[0],-pos[1],110-pos[2]
            rotation=unreal.Rotator(math.degrees(math.atan2(dz,math.hypot(dx,dy))),math.degrees(math.atan2(dy,dx)),0)
            camera=spawn(actors,label,unreal.CineCameraActor,pos,rotation)
            settings(camera,focal,aperture)
        if not levels.save_current_level(): raise RuntimeError("电影镜头地图无法保存")
        map_state="created"
    allactors={a.get_actor_label():a for a in actors.get_all_level_actors()}
    for label,focal,aperture,_ in VIEWS:
        actor=allactors.get(label)
        if not isinstance(actor,unreal.CineCameraActor): raise RuntimeError("缺少镜头Actor: "+label)
        actual=float(actor.get_cine_camera_component().get_editor_property("current_focal_length"))
        if abs(actual-focal)>0.05: raise RuntimeError("已保存镜头焦距不匹配:"+label)
    record={"engine":str(unreal.SystemLibrary.get_engine_version()),"map":MAP,"map_state":map_state,
            "blueprint":BP,"blueprint_state":bp_state,"actor_count":len(allactors),
            "focal_mm":{label:focal for label,focal,_,_ in VIEWS},
            "manual_review":"pending","runtime":"not_run"}
    dst=ROOT/"Saved/Validation/CinematicCamera/AuthorCinematicCameraReview.json"
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CINEMATIC_AUTHOR_OK",json.dumps(record,ensure_ascii=False))
if __name__=="__main__": main()