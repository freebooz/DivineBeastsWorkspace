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
def look_at(point, target=(0,0,110)):
    """UE Python Rotator参数为(roll,pitch,yaw)，不可沿用C++ FRotator顺序。"""
    import math
    dx,dy,dz=target[0]-point[0],target[1]-point[1],target[2]-point[2]
    yaw=math.degrees(math.atan2(dy,dx))
    pitch=math.degrees(math.atan2(dz,math.hypot(dx,dy)))
    return unreal.Rotator(0.0,pitch,yaw)

def camera_facing_target(actor, target=(0,0,110)):
    origin=actor.get_actor_location()
    f=actor.get_actor_forward_vector()
    dx,dy,dz=target[0]-origin.x,target[1]-origin.y,target[2]-origin.z
    import math
    length=math.sqrt(dx*dx+dy*dy+dz*dz)
    return length>1.e-6 and (f.x*dx+f.y*dy+f.z*dz)/length > .999
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
    film.sensor_height=20.25  # 宽高比16:9，开发截图不人为裁成超宽电影条幅。
    comp.set_editor_property("filmback",film)
    comp.set_editor_property("current_focal_length",focal)
    comp.set_editor_property("current_aperture",aperture)
    focus=comp.get_editor_property("focus_settings")
    focus.focus_method=unreal.CameraFocusMethod.MANUAL
    import math
    loc=actor.get_actor_location()
    # 当前相机到角色头部附近的真实厘米距离，避免各焦距使用固定380cm导致离焦。
    focus.manual_focus_distance=math.sqrt(loc.x*loc.x+loc.y*loc.y+(loc.z-110.0)**2)
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
            spawn(actors,"DBA_CameraReview_Sun",unreal.DirectionalLight,(-900,-900,1500),unreal.Rotator(0,-42,15)); changed=True
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
                rotation=look_at(pos)
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
        spawn(actors,"DBA_CameraReview_Sun",unreal.DirectionalLight,(-900,-900,1500),unreal.Rotator(0,-42,15))
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
            rotation=look_at(pos)
            camera=spawn(actors,label,unreal.CineCameraActor,pos,rotation)
            settings(camera,focal,aperture)
        if not levels.save_current_level(): raise RuntimeError("电影镜头地图无法保存")
        map_state="created"
    allactors={a.get_actor_label():a for a in actors.get_all_level_actors()}
    # 纠正曾按C++参数顺序创建的四个镜头，其他地图资源不受影响。
    rotation_repaired=False
    for label,focal,aperture,pos in VIEWS:
        existing=allactors.get(label)
        if existing:
            if not camera_facing_target(existing):
                existing.set_actor_rotation(look_at(pos),False)
                rotation_repaired=True
            import math
            target_focus=math.sqrt(pos[0]**2+pos[1]**2+(pos[2]-110.0)**2)
            current_focus=existing.get_cine_camera_component().get_editor_property("focus_settings").manual_focus_distance
            height=float(existing.get_cine_camera_component().get_editor_property("filmback").sensor_height)
            if abs(float(current_focus)-target_focus)>1.0 or abs(height-20.25)>0.1:
                settings(existing,focal,aperture)
                rotation_repaired=True
    if rotation_repaired and not levels.save_current_level():
        raise RuntimeError("镜头朝向修复未能保存")
    # 电影镜头采用物理曝光；开发场景必须提供室外日光照度，否则游戏实拍全黑。
    sun=allactors.get("DBA_CameraReview_Sun")
    if sun:
        sun_component=sun.get_component_by_class(unreal.DirectionalLightComponent)
        needs_light_save=False
        # 把旧的-roll/-pitch误传旋转改为真正的俯仰-42度，保留暖色白昼照明。
        sun_rot=sun.get_actor_rotation()
        if abs(float(sun_rot.pitch)+42.0)>0.1 or abs(float(sun_rot.yaw)-15.0)>0.1 or abs(float(sun_rot.roll))>0.1:
            sun.set_actor_rotation(unreal.Rotator(0.0,-42.0,15.0),False)
            needs_light_save=True
        # 初始镜头f/5.6、1/60秒无需65000lux，避免物理曝光过亮。
        if sun_component and abs(float(sun_component.get_editor_property("intensity"))-500.0)>1.0:
            sun_component.set_editor_property("intensity",500.0)
            needs_light_save=True
        if needs_light_save and not levels.save_current_level():
            raise RuntimeError("开发场景室外光照修复未保存")
    # 游戏运行会自动生成DefaultPawn球体；开发地图将其出生点放到演示机位之外。
    # 正式Village/竞技出生和本地账号流程完全不受影响。
    player_start=allactors.get("DBA_CameraReview_Start")
    if player_start and player_start.get_actor_location().x > -3000.0:
        if not player_start.set_actor_location(unreal.Vector(-5000.0,0.0,160.0),False,False):
            raise RuntimeError("开发地图默认Pawn出生点移动失败")
        if not levels.save_current_level():
            raise RuntimeError("默认Pawn出生点修改未保存")
    # 真正游戏运行时默认显示35mm电影机位；其它三种机位用于编辑器比较。
    main_cine=allactors.get("DBA_Cine_35mm")
    if main_cine and main_cine.get_editor_property("auto_activate_for_player") != unreal.AutoReceiveInput.PLAYER0:
        main_cine.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
        if not levels.save_current_level(): raise RuntimeError("保存默认电影机位失败")
    # 真实使用已生成的蓝图Actor作为游戏模式下的默认机位；
    # 原生四机位继续保留供独立焦距比较，不删除或重命名已发布资产。
    blueprint_camera=allactors.get("DBA_Cine_BlueprintRig")
    needs_rig_save=False
    if blueprint_camera is None:
        camera_class=assets.load_blueprint_class(BP)
        if camera_class is None: raise RuntimeError("真实CineCamera蓝图类加载失败")
        pos=(-850.0,-200.0,230.0)
        blueprint_camera=spawn(actors,"DBA_Cine_BlueprintRig",camera_class,pos,look_at(pos))
        settings(blueprint_camera,35.0,5.6)
        allactors["DBA_Cine_BlueprintRig"]=blueprint_camera
        needs_rig_save=True
    if not isinstance(blueprint_camera,unreal.CineCameraActor):
        raise RuntimeError("镜头蓝图实例不是CineCameraActor")
    if "BP_DBA_CinematicCameraRig" not in blueprint_camera.get_class().get_name():
        raise RuntimeError("演示地图并未使用真实镜头蓝图类")
    if blueprint_camera.get_editor_property("auto_activate_for_player") != unreal.AutoReceiveInput.PLAYER0:
        blueprint_camera.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.PLAYER0)
        needs_rig_save=True
    original=allactors.get("DBA_Cine_35mm")
    if original and original.get_editor_property("auto_activate_for_player") != unreal.AutoReceiveInput.DISABLED:
        original.set_editor_property("auto_activate_for_player",unreal.AutoReceiveInput.DISABLED)
        needs_rig_save=True
    if needs_rig_save and not levels.save_current_level():
        raise RuntimeError("真实电影摄像机蓝图Actor保存失败")
    for label,focal,aperture,_ in VIEWS:
        actor=allactors.get(label)
        if not isinstance(actor,unreal.CineCameraActor): raise RuntimeError("缺少镜头Actor: "+label)
        if not camera_facing_target(actor): raise RuntimeError("电影镜头未朝向展示角色: "+label)
        actual=float(actor.get_cine_camera_component().get_editor_property("current_focal_length"))
        if abs(actual-focal)>0.05: raise RuntimeError("已保存镜头焦距不匹配:"+label)
    record={"engine":str(unreal.SystemLibrary.get_engine_version()),"map":MAP,"map_state":map_state,
            "blueprint":BP,"blueprint_state":bp_state,"actor_count":len(allactors),
            "blueprint_actor":"DBA_Cine_BlueprintRig",
            "focal_mm":{label:focal for label,focal,_,_ in VIEWS},
            "manual_review":"pending","runtime":"not_run"}
    dst=ROOT/"Saved/Validation/CinematicCamera/AuthorCinematicCameraReview.json"
    dst.parent.mkdir(parents=True,exist_ok=True)
    dst.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
    print("SSLM_CINEMATIC_AUTHOR_OK",json.dumps(record,ensure_ascii=False))
if __name__=="__main__": main()