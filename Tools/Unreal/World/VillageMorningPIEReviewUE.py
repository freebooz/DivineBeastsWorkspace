"""隔离审核的PIE一次性引导；不保存资产，不冒充生产GamePlatformData租约。

截图与性能分开会话。相机移动交给引擎原生MoveComponentTo，不用Python逐帧驱动。
"""
import json
import re
from datetime import datetime, timezone
from pathlib import Path
import unreal as u
import ReviewVillageMorningEnvironmentUE as author

TEXTURES={
 'SnowCoverageTexture':'VillageSnowCoverageTexture',
 'SnowAlbedoTexture':'VillageSnowAlbedoTexture',
 'SnowNormalTexture':'VillageSnowNormalTexture',
 'SnowORMTexture':'VillageSnowORMTexture',
 'SnowHeightTexture':'VillageSnowHeightTexture',
 'GroundAlbedoTexture':'VillageGroundAlbedoTexture',
 'GroundNormalTexture':'VillageGroundNormalTexture',
 'GroundORMTexture':'VillageGroundORMTexture'}
HELD=[]
CAMERA=None
RUN=None

def game():
    w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
    assert w and w.get_outermost().get_name().startswith(author.REVIEW+'/Maps/UEDPIE_'), '只允许隔离PIE审核世界；禁止正式/其他地图'
    short=re.sub(r'^UEDPIE_\d+_', '',w.get_outermost().get_name().rsplit('/',1)[1])
    assert short in ['L_Village_Baseline_Overcast']+['L_'+k for k in author.presets()['presets']], '不在审核地图白名单'
    return w

def bind_landscape(w):
    settings_class=u.load_class(None,'/Script/DivineBeastsPresentationClient.DivineBeastsVillageSnowSettings')
    assert settings_class, '正式设置反射类缺失'
    settings=u.get_default_object(settings_class)
    # 非BlueprintType设置只有原生FProperty名；SoftObjectPath的真实export_text为完整对象路径。
    loaded={param:u.load_asset(settings.get_editor_property(prop).export_text()) for prop,param in TEXTURES.items()}
    assert all(loaded.values()), '八纹理加载失败'
    HELD.extend(loaded.values())
    results=[]
    for land in u.GameplayStatics.get_all_actors_of_class(w,u.LandscapeProxy):
        assert land.get_editor_property('use_dynamic_material_instance')
        assert land.get_editor_property('landscape_material').get_path_name().startswith('/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge')
        for param,texture in loaded.items(): land.set_landscape_material_texture_parameter_value(param,texture)
        land.set_landscape_material_vector_parameter_value('VillageDryColor',u.LinearColor(1,1,1,1))
        land.set_landscape_material_vector_parameter_value('VillageWetColor',u.LinearColor(.55,.60,.52,1))
        for param in ['VillageSnowAmount','VillageWetness']: land.set_landscape_material_scalar_parameter_value(param,0.)
        mids=[]
        for comp in land.get_components_by_class(u.LandscapeComponent):
            for i in range(comp.get_num_materials()):
                mid=comp.get_material_instance_dynamic(i)
                assert mid, '真实Landscape MID缺失'
                for param,texture in loaded.items(): assert mid.get_texture_parameter_value(param)==texture, '纹理回读不一致: '+param
                dry=mid.get_vector_parameter_value('VillageDryColor')
                assert min(dry.r,dry.g,dry.b)>.999
                assert abs(mid.get_scalar_parameter_value('VillageSnowAmount'))<.001
                mids.append(mid)
        slots=0
        nanites=[c for c in land.get_components_by_class(u.StaticMeshComponent) if c.get_class().get_name()=='LandscapeNaniteComponent']
        short=re.sub(r'^UEDPIE_\d+_', '',w.get_outermost().get_name().rsplit('/',1)[1])
        recorded=json.loads((author.OUT/'NativeNaniteBindings.json').read_text())[short]
        live_sources={c.get_name():c for c in land.get_components_by_class(u.LandscapeComponent)}
        for nanite in nanites:
            # 此protected属性Python拒读；顺序来自Monolith原生反射实际回读，绝非Actor数组顺序。
            sources=[live_sources[n] for n in recorded[nanite.get_name()]]
            assert sources and len(sources)==nanite.get_num_materials(), 'Nanite实际映射不完整；不能猜序'
            for slot,source in enumerate(sources):
                mid=source.get_material_instance_dynamic(0); assert mid
                nanite.set_material(slot,mid)
                assert nanite.get_material(slot)==mid
                slots+=1
        results.append({'landscape':land.get_name(),'mids':len(mids),'nanite_slots':slots})
    assert results
    return {'binding':results,'textures':{p:t.get_path_name() for p,t in loaded.items()},'loader':'Explicit Editor-only synchronous load; production lease not tested'}

def set_camera(view):
    global CAMERA
    w=game(); spec=json.loads((author.OUT/'SpatialLayout.json').read_text())['cameras'][view]
    loc=u.Vector(*spec['location']); target=u.Vector(*spec['target'])
    rot=u.MathLibrary.find_look_at_rotation(loc,target)
    if CAMERA is None or not u.SystemLibrary.is_valid(CAMERA):
        candidates=[a for a in u.GameplayStatics.get_all_actors_of_class(w,u.CameraActor) if a.get_name()=='CameraActor_0']
        assert len(candidates)==1, '必须复用场景已存在的审核相机'
        CAMERA=candidates[0]
    CAMERA.set_actor_location_and_rotation(loc,rot,False,False)
    CAMERA.get_component_by_class(u.CameraComponent).set_field_of_view(75.)
    u.GameplayStatics.get_player_controller(w,0).set_view_target_with_blend(CAMERA,0.)
    return {'view':view,'location':spec['location'],'target':spec['target'],'fov':75.}

def setup(run,quality='High',view='LakeShore'):
    global CAMERA,RUN
    CAMERA=None; HELD.clear(); RUN=run
    w=game()
    # P5运行前后都核对真实PIE曝光；避免把旧+2.5EV图误作新正式+1.0EV对照。
    exposure_bias=None
    if author.REVIEW == author.P5_REVIEW:
        volumes=u.GameplayStatics.get_all_actors_of_class(w,u.PostProcessVolume)
        assert len(volumes)==1, 'P5审核地图中的后期处理体积数量异常'
        pp=volumes[0].get_editor_property('settings')
        exposure_bias=pp.get_editor_property('auto_exposure_bias')
        expected=author.presets()['shared']['fixed_exposure_bias']
        assert bool(pp.get_editor_property('override_auto_exposure_bias')), 'P5后期曝光补偿未启用'
        assert abs(exposure_bias-expected)<.001, 'P5曝光补偿与当前正式基线不一致'
    # 显式调用既有平台权威入口，仅对本地PIE世界应用既有Clear定义，保留风.70。
    preset=u.load_asset('/DBAWorldPack_Village/Weather/Definitions/DA_DBA_Weather_Clear')
    HELD.append(preset)
    applied=u.GamePlatformWeatherBlueprintLibrary.apply_weather_preset_on_authority(w,preset)
    assert applied, '审核PIE未接纳既有Clear预设'
    q=author.presets()['quality'][quality]
    for group in ['ViewDistance','AntiAliasing','Shadow','GlobalIllumination','Reflection','PostProcess','Texture','Effects','Foliage','Shading']:
        u.SystemLibrary.execute_console_command(w,'sg.'+group+'Quality '+str(q['scalability']))
    for key,value in q.items():
        if key!='scalability': u.SystemLibrary.execute_console_command(w,key+' '+str(value))
    # 0仍触发UEditorEngine的120 FPS隐式夹取；本轮临时1000高上限，结束恢复0。
    for cmd in ['t.MaxFPS 1000','r.VSync 0','r.DynamicRes.OperationMode 0','r.ScreenPercentage 100','r.GPUCsvStatsEnabled 1']:
        u.SystemLibrary.execute_console_command(w,cmd)
    camera=set_camera(view)
    landscape=bind_landscape(w)
    viewport=u.WidgetLayoutLibrary.get_viewport_size(w)
    report={'utc':datetime.now(timezone.utc).isoformat(),'world':w.get_path_name(),'quality':quality,'viewport':[viewport.x,viewport.y],'landscape':landscape,'camera':camera,'clear_preset_applied':True,'runtime_wind_definition':.70,'production_village_assembly_active':False,'formal_merge_approved':False,'exposure_bias_ev':exposure_bias,'review_scope':'p5' if author.REVIEW==author.P5_REVIEW else 'legacy','tMaxFPS':u.SystemLibrary.get_console_variable_float_value('t.MaxFPS')}
    author.write(run+'_Setup.json',report)
    print('MORNING_PIE_READY '+json.dumps({'run':run,'mids':sum(x['mids'] for x in landscape['binding']),'nanite_slots':sum(x['nanite_slots'] for x in landscape['binding'])}))

def move_camera(seconds=30.0):
    assert CAMERA and u.SystemLibrary.is_valid(CAMERA)
    comp=CAMERA.get_editor_property('RootComponent')
    comp.set_mobility(u.ComponentMobility.MOVABLE)
    pos=CAMERA.get_actor_location(); end=u.Vector(pos.x+1800,pos.y+800,pos.z+100)
    latent=u.LatentActionInfo()
    latent.import_text('(Linkage=0,UUID=1729,ExecutionFunction=None,CallbackTarget="'+CAMERA.get_path_name()+'")')
    assert CAMERA.get_path_name() in latent.export_text(), '引擎LatentActionInfo目标回读失败'
    u.SystemLibrary.move_component_to(comp,end,CAMERA.get_actor_rotation(),False,False,seconds,False,u.MoveComponentAction.MOVE,latent)
    print('MORNING_NATIVE_CAMERA_MOVE',seconds)

def snapshot(suffix):
    w=game(); rows=[]
    for a in u.GameplayStatics.get_all_actors_of_class(w,u.ExponentialHeightFog):
        c=a.get_component_by_class(u.ExponentialHeightFogComponent)
        rows.append(author.read_props(c,author.PROPS['ExponentialHeightFogComponent']))
    mpc=u.load_asset('/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal')
    wind=u.MaterialLibrary.get_vector_parameter_value(w,mpc,'GlobalWindTo')
    suns=[{'rotation':a.get_actor_rotation().export_text(),'component':author.read_props(a.get_component_by_class(u.DirectionalLightComponent),author.PROPS['DirectionalLightComponent'])} for a in u.GameplayStatics.get_all_actors_of_class(w,u.DirectionalLight)]
    exposure=[a.get_editor_property('settings').get_editor_property('auto_exposure_bias') for a in u.GameplayStatics.get_all_actors_of_class(w,u.PostProcessVolume)]
    if author.REVIEW == author.P5_REVIEW:
        assert len(exposure)==1, 'P5运行时后期处理体积数量异常'
        assert abs(exposure[0]-author.presets()['shared']['fixed_exposure_bias'])<.001, 'P5运行时曝光偏离基线'
    author.write(RUN+'_'+suffix+'.json',{'utc':datetime.now(timezone.utc).isoformat(),'world':w.get_path_name(),'exposure_bias_ev':exposure,'fog':rows,'sun':suns,'camera_location':author.xyz(CAMERA.get_actor_location()),'wind':[wind.r,wind.g,wind.b,wind.a],'cvars':{key:u.SystemLibrary.get_console_variable_int_value(key) for key in ['r.VolumetricFog','r.VolumetricFog.GridPixelSize','r.VolumetricFog.GridSizeZ','r.DynamicGlobalIlluminationMethod','r.ReflectionMethod']}})

def cleanup():
    global CAMERA,RUN
    HELD.clear(); CAMERA=None; RUN=None
