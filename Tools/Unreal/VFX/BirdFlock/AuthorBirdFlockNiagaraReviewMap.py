#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""独立UE命令行编辑器生成真实GPU鸟群审核地图，不切换正在使用的主编辑器关卡。
本地图以NiagaraActor直接演示平台资产，是开发审核夹具，不作为正式游戏执行器。
正式游戏依然走项目场景组件->中立表现请求->GamePlatformVFX唯一执行链。
"""
import json
import os
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[4]
MAP='/Game/Development/BirdFlock/Maps/L_DBA_BirdFlockReview'
SYSTEMS='/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara'

def main():
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsArena.uproject')
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    assets=unreal.EditorAssetLibrary
    levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if assets.does_asset_exist(MAP):
        assert isinstance(assets.load_asset(MAP),unreal.World)
        print('BIRD_GPU_REVIEW_MAP_ALREADY_EXISTS',MAP)
        return
    # 脚本只准独立命令行作者进程写入新地图，不通过Monolith切走其他任务的编辑状态。
    if os.getenv('BIRD_ISOLATED_REVIEW_AUTHOR')!='1':
        raise RuntimeError('必须由独立的UE编辑器命令行明确授权BIRD_ISOLATED_REVIEW_AUTHOR=1')
    loaded={s:assets.load_asset(SYSTEMS+'/NS_DBA_BirdFlock_'+s) for s in ('Seagull','Swallow','Eagle')}
    assert all(isinstance(x,unreal.NiagaraSystem) for x in loaded.values())
    assets.make_directory(MAP.rsplit('/',1)[0])
    assert levels.new_level(MAP)
    def spawn(cls,label,location,rotation=(0,0,0)):
        item=actors.spawn_actor_from_class(cls,unreal.Vector(*location),unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]))
        assert item is not None, label
        item.set_actor_label(label)
        return item
    report=[]
    for s,x,ground,count in (('Seagull',-5000,300,32),('Swallow',0,450,72),('Eagle',5000,200,5)):
        obj=spawn(unreal.NiagaraActor,'BirdReview_'+s,(x,0,ground))
        component=obj.get_component_by_class(unreal.NiagaraComponent)
        assert component
        component.set_asset(loaded[s])
        component.set_variable_int('User.BirdCount',count)
        component.set_editor_property('auto_activate',True)
        report.append({'species':s,'count':count,'system':loaded[s].get_path_name(),'actor':obj.get_name()})
    sun=spawn(unreal.DirectionalLight,'BirdReview_Sun',(0,-2000,6500),(-45,30,0))
    sun.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',5.0)
    spawn(unreal.SkyAtmosphere,'BirdReview_SkyAtmosphere',(0,0,0))
    sky=spawn(unreal.SkyLight,'BirdReview_SkyLight',(0,0,2000))
    sky.get_component_by_class(unreal.SkyLightComponent).set_editor_property('intensity',1.0)
    floor=spawn(unreal.StaticMeshActor,'BirdReview_Floor',(0,0,-100))
    floor.static_mesh_component.set_static_mesh(assets.load_asset('/Engine/BasicShapes/Plane'))
    floor.set_actor_scale3d(unreal.Vector(300,220,1))
    floor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    camera=spawn(unreal.CameraActor,'BirdReview_Camera',(0,-14000,7000),(-22,90,0))
    camera.set_editor_property('auto_activate_for_player',unreal.AutoReceiveInput.PLAYER0)
    camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(65.0)
    spawn(unreal.PlayerStart,'BirdReview_PlayerStart',(0,-13500,6500),(-20,90,0))
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
    assert levels.save_current_level()
    assert isinstance(assets.load_asset(MAP),unreal.World)
    evidence=ROOT/'Saved/Validation/BirdFlockP4';evidence.mkdir(parents=True,exist_ok=True)
    (evidence/'ReviewMapManifest.json').write_text(json.dumps({'map':MAP,'gpuFlocks':report,'manualAcceptance':'pending'},ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_GPU_REVIEW_MAP_SAVED',MAP,len(report))

if __name__=='__main__':
    main()
