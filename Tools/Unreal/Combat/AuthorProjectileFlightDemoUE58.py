"""Monolith/UE原生创建可重复运行的开发演示地图；只布置配置发射夹具，不实现运动。"""
from pathlib import Path
import json,uuid
import unreal as u

root=Path(u.Paths.project_dir()).resolve().parent
cfg=json.loads((root/'Tools/Unreal/Combat/ProjectileFlightProfiles.json').read_text('utf-8'))
path=cfg['demo_directory']+'/L_DBA_ProjectileMotion_Demo'
current=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
is_current=current and current.get_path_name().split('.')[0]==path
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages() or is_current,'其它地图有未保存内容'
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
level=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
if is_current or u.EditorAssetLibrary.does_asset_exist(path):
    if not is_current:assert level.load_level(path)
    for actor in actors.get_all_level_actors():
        if 'SSLM.ProjectileMotion.Demo' in [str(t) for t in actor.tags]:actors.destroy_actor(actor)
else:
    assert level.new_level(path)
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase.static_class())
demo=u.load_class(None,'/Script/DivineBeastsAbilitiesRuntime.DivineBeastsProjectileMotionDemo');assert demo

def spawn(cls,location,label,rotation=u.Rotator()):
    actor=actors.spawn_actor_from_class(cls,location,rotation)
    assert actor,label
    actor.set_actor_label(label);actor.set_editor_property('tags',['SSLM.ProjectileMotion.Demo'])
    return actor

floor=spawn(u.StaticMeshActor,u.Vector(1600,0,-15),'ProjectileMotion_DemoFloor')
mesh=floor.get_component_by_class(u.StaticMeshComponent)
mesh.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(u.Vector(60,36,.2))
light=spawn(u.DirectionalLight,u.Vector(0,0,1500),'ProjectileMotion_DemoLight',u.Rotator(-50,-30,0))
for index,row in enumerate(cfg['profiles']):
    lane=(index-3)*450
    actor=spawn(demo,u.Vector(0,lane,160),'ProjectileMotion_'+row['name'])
    profile_path=cfg['fireball_profile'] if row['name']=='Fireball' else cfg['demo_directory']+'/DA_DBA_ProjectileFlight_'+row['name']
    actor.set_editor_property('flight_profile',u.load_asset(profile_path))
    actor.set_editor_property('bShowDebugMesh',True)
    if row['name']=='Parabolic':actor.set_editor_property('launch_direction',u.Vector(.94,0,.342))
    if row.get('b_enable_homing',False):
        target=spawn(demo,u.Vector(1800,lane+200,160),'ProjectileMotion_Target_'+row['name'])
        target.get_editor_property('root_component').set_collision_enabled(u.CollisionEnabled.QUERY_ONLY)
        target.get_editor_property('root_component').set_collision_response_to_all_channels(u.CollisionResponseType.ECR_BLOCK)
        marker=spawn(u.StaticMeshActor,target.get_actor_location(),'ProjectileMotion_TargetMarker_'+row['name'])
        visual=marker.get_component_by_class(u.StaticMeshComponent)
        visual.set_mobility(u.ComponentMobility.MOVABLE)
        visual.set_static_mesh(u.load_asset('/Engine/BasicShapes/Sphere'))
        visual.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        marker.set_actor_scale3d(u.Vector(.6,.6,.6))
        assert marker.attach_to_actor(target,'',u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,u.AttachmentRule.KEEP_WORLD,False)
        actor.set_editor_property('target',target)
        if row['name']=='Homing':actor.set_editor_property('launch_direction',u.Vector(0,1,0))
    label=spawn(u.TextRenderActor,u.Vector(-50,lane,260),'ProjectileMotion_Label_'+row['name'],u.Rotator(0,-90,0))
    label.get_component_by_class(u.TextRenderComponent).set_text(row['name'])
camera=spawn(u.CameraActor,u.Vector(2400,-4200,3200),'ProjectileMotion_DemoCamera')
camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(camera.get_actor_location(),u.Vector(1800,0,160)),False)
camera.set_editor_property('auto_activate_for_player',u.AutoReceiveInput.PLAYER0)
spawn(u.PlayerStart,camera.get_actor_location(),'ProjectileMotion_DemoStart',camera.get_actor_rotation())
assert u.EditorLoadingAndSavingUtils.save_map(world,path)
out=root/'Saved/Validation/ProjectileMotion20261101'/('Demo-'+str(uuid.uuid4()));out.mkdir(parents=True)
(out/'Map.json').write_text(json.dumps({'map':path,'lanes':[r['name'] for r in cfg['profiles']],
    'actor_count':len(actors.get_all_level_actors()),'saved':True,'pie_verified':False,'network_verified':False},indent=2),'utf-8')
print(json.dumps({'status':'Saved','report':str(out/'Map.json')}))
