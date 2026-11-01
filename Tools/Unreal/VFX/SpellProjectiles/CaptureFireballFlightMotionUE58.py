# -*- coding: utf-8 -*-
"""Monolith原生移动RHI采样；使用真实编辑器帧，清理全部临时Actor，不保存地图。

固定3000cm/s仅为表现夹具；30fps为媒体采样，不限制游戏或编辑器帧率。
"""
from pathlib import Path
import json
import uuid
import hashlib
import math
import os
import unreal as u

ROOT=Path(__file__).resolve().parents[4]
CONFIG=json.loads((Path(__file__).with_name('PresentationManifest.json')).read_text('utf-8'))['fireball_appearance']
OUT=ROOT/'Saved/Validation/FireballTrail20261101/NativeMotion'

def capture_review(view, quality, frame_count=48):
    """独立代次原生材质审核；只使用本轮Transient Actor/MID，不保存地图或偏好。"""
    assert '-SSLMFireballMaterialsOwned' in u.SystemLibrary.get_command_line()
    assert view in ('core-side','core-oblique','core-front')
    assert 1<=frame_count<=120
    u.AutomationLibrary.finish_loading_before_screenshot()
    run_id=str(uuid.uuid4());out=ROOT/'Saved/Validation/FireballFlight'/run_id/'Reviews'/view
    out.mkdir(parents=True,exist_ok=False)
    sub=u.get_editor_subsystem(u.EditorActorSubsystem)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    original_quality=u.SystemLibrary.get_console_variable_int_value('fx.Niagara.QualityLevel')
    settings=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
    original_throttle=settings.get_editor_property('bThrottleCPUWhenNotForeground')
    state={'run_id':run_id,'view':view,'quality':quality,'status':'Preparing','frames':[],
           'frame_count':frame_count,'handle':None,'elapsed':0.,'since_sample':0.,'warmup':0,
           'core_diameter_cm':CONFIG['head_diameter_cm'],'source':'UE5.8 native SceneCapture2D',
           'manual_user_accepted':False,'engine_frame_cap_changed':False}
    content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/SpellProjectiles'
    packages=['Materials/Fireball/M_DBA_Fireball_CoreSurface.uasset','Materials/Fireball/MI_DBA_Fireball_CoreSurface.uasset',
              'Textures/Fireball/T_DBA_Fireball_CoreFlow.uasset','Meshes/Fireball/SM_DBA_Fireball_CoreSphere.uasset']
    state['asset_hashes']={p:hashlib.sha256((content/p).read_bytes()).hexdigest() for p in packages}
    state['preview_overrides']={'UseNiagaraAge':0,'auto_exposure':'Manual','exposure_bias':0}
    state['fps_state']={'t.MaxFPS':u.SystemLibrary.get_console_variable_float_value('t.MaxFPS'),
                        'r.VSync':u.SystemLibrary.get_console_variable_int_value('r.VSync')}
    assert state['fps_state']['t.MaxFPS']==0 and state['fps_state']['r.VSync']==0
    ball=camera=None
    def save():
        (out/'Capture.json').write_text(json.dumps({k:v for k,v in state.items() if k!='handle'},indent=2),encoding='utf-8')
    def finish(error=None):
        if state['handle'] is not None:u.unregister_slate_post_tick_callback(state['handle'])
        if camera:sub.destroy_actor(camera)
        if ball:sub.destroy_actor(ball)
        settings.set_editor_property('bThrottleCPUWhenNotForeground',original_throttle)
        u.SystemLibrary.execute_console_command(world,'fx.Niagara.QualityLevel '+str(original_quality))
        state['status']='Failed' if error else 'CompletePendingHumanReview'
        if error:state['error']=str(error)
        save()
    try:
        settings.set_editor_property('bThrottleCPUWhenNotForeground',False)
        u.SystemLibrary.execute_console_command(world,'fx.Niagara.QualityLevel '+str(quality))
        ball=sub.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,150),transient=True)
        component=ball.get_component_by_class(u.StaticMeshComponent)
        component.set_static_mesh(u.load_asset(CONFIG['head_mesh']))
        diameter=CONFIG['head_diameter_cm'];ball.set_actor_scale3d(u.Vector(diameter/100,diameter/100,diameter/100))
        mi=u.load_asset('/DBAPresentationPack_Core/VFX/SpellProjectiles/Materials/Fireball/MI_DBA_Fireball_CoreSurface')
        assert mi and mi.get_editor_property('parent').get_editor_property('blend_mode')==u.BlendMode.BLEND_OPAQUE
        component.set_material(0,mi)
        preview=component.create_dynamic_material_instance(0,mi)
        preview.set_scalar_parameter_value('UseNiagaraAge',0.)
        location={'core-side':u.Vector(0,260,150),'core-front':u.Vector(260,0,150),'core-oblique':u.Vector(180,180,210)}[view]
        camera=sub.spawn_actor_from_class(u.SceneCapture2D,location,transient=True)
        camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(location,u.Vector(0,0,150)),False)
        cc=camera.get_component_by_class(u.SceneCaptureComponent2D)
        target=u.RenderingLibrary.create_render_target2d(world,1280,720,u.TextureRenderTargetFormat.RTF_RGBA8)
        cc.set_editor_property('texture_target',target)
        cc.set_editor_property('capture_every_frame',False);cc.set_editor_property('capture_on_movement',False)
        cc.set_editor_property('capture_source',u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cc.set_editor_property('primitive_render_mode',u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
        cc.show_only_actor_components(ball);cc.set_editor_property('fov_angle',40.)
        cc.set_editor_property('show_flag_settings',[u.EngineShowFlagsSetting(show_flag_name=n,enabled=False) for n in ['Atmosphere','Fog','VolumetricFog','VolumetricCloud','SkyLighting','MotionBlur']])
        pp=cc.get_editor_property('post_process_settings')
        pp.set_editor_property('override_auto_exposure_method',True)
        pp.set_editor_property('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL)
        pp.set_editor_property('override_auto_exposure_bias',True);pp.set_editor_property('auto_exposure_bias',0.)
        cc.set_editor_property('post_process_settings',pp);cc.set_editor_property('post_process_blend_weight',1.)
        state['camera']=[location.x,location.y,location.z];state['status']='Capturing';save()
        def tick(delta):
            try:
                state['elapsed']+=delta;state['since_sample']+=delta
                if state['since_sample']<1/30:return
                state['since_sample']=0.;state['warmup']+=1
                if state['warmup']<=12:return
                cc.capture_scene();name='Frame%03d.png'%len(state['frames'])
                u.RenderingLibrary.export_render_target(world,target,str(out),name)
                assert (out/name).is_file() and (out/name).stat().st_size>1000
                state['frames'].append({'file':str(out/name),'time':state['elapsed']});save()
                if len(state['frames'])>=frame_count:finish()
            except Exception as error:finish(error)
        state['handle']=u.register_slate_post_tick_callback(tick)
    except Exception as error:
        finish(error);raise
    print('MATERIAL_REVIEW_STARTED',str(out/'Capture.json'))
    return {'run_id':run_id,'report':str(out/'Capture.json')}


def capture_flight_review(speed=3000.,quality=2,frame_count=48,light_enabled=True,trajectory='straight',drain=False):
    """真实帧移动和原生Niagara模拟，固定镜头相对弹体；媒体采样不限制引擎帧率。"""
    assert '-SSLMFireballMaterialsOwned' in u.SystemLibrary.get_command_line()
    u.AutomationLibrary.finish_loading_before_screenshot()
    run=str(uuid.uuid4());out=ROOT/'Saved/Validation/FireballFlight'/run/'Reviews'/'flight';out.mkdir(parents=True)
    sub=u.get_editor_subsystem(u.EditorActorSubsystem);world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    perf=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'));throttle=perf.get_editor_property('bThrottleCPUWhenNotForeground');perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
    oldq=u.SystemLibrary.get_console_variable_int_value('fx.Niagara.QualityLevel');u.SystemLibrary.execute_console_command(world,'fx.Niagara.QualityLevel '+str(quality))
    oldlight=u.SystemLibrary.get_console_variable_int_value('fx.EnableNiagaraLightRendering');u.SystemLibrary.execute_console_command(world,'fx.EnableNiagaraLightRendering '+str(int(light_enabled)))
    ball=sub.spawn_actor_from_class(u.NiagaraActor,u.Vector(0,0,100),transient=True)
    comp=ball.get_editor_property('niagara_component');comp.set_force_solo(True);comp.set_asset(u.load_asset('/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight'));comp.activate(True);comp.set_paused(True)
    floor=sub.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,0),transient=True);fc=floor.get_component_by_class(u.StaticMeshComponent);fc.set_static_mesh(u.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(u.Vector(16,6,1))
    camera=sub.spawn_actor_from_class(u.SceneCapture2D,u.Vector(-180,700,330),transient=True)
    cc=camera.get_component_by_class(u.SceneCaptureComponent2D);rt=u.RenderingLibrary.create_render_target2d(world,1280,720,u.TextureRenderTargetFormat.RTF_RGBA8)
    cc.set_editor_property('texture_target',rt);cc.set_editor_property('capture_source',u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    cc.set_editor_property('capture_every_frame',False);cc.set_editor_property('capture_on_movement',False)
    cc.set_editor_property('fov_angle',math.degrees(2*math.atan((CONFIG['trail_length_cm']*.5+CONFIG['shell_diameter_cm'])*1.15/700.)))
    cc.set_editor_property('primitive_render_mode',u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST);cc.show_only_actor_components(ball);cc.show_only_actor_components(floor)
    cc.set_editor_property('show_flag_settings',[u.EngineShowFlagsSetting(show_flag_name=n,enabled=False) for n in ['Atmosphere','Fog','VolumetricFog','VolumetricCloud','MotionBlur']])
    pp=cc.get_editor_property('post_process_settings');pp.set_editor_property('override_auto_exposure_method',True);pp.set_editor_property('auto_exposure_method',u.AutoExposureMethod.AEM_MANUAL);pp.set_editor_property('override_auto_exposure_bias',True);pp.set_editor_property('auto_exposure_bias',0.);cc.set_editor_property('post_process_settings',pp);cc.set_editor_property('post_process_blend_weight',1.)
    assert trajectory in ('straight','turn','network-step')
    state={'run_id':run,'pid':os.getpid(),'status':'Capturing','frames':[],'frame_times':[],'elapsed':0.,'quality':quality,'speed_cm_s':speed,'light_enabled':light_enabled,'trajectory':trajectory,'drain_requested':drain,'human_accepted':False,'engine_cap':u.SystemLibrary.get_console_variable_float_value('t.MaxFPS'),'handle':None,'x':0.,'y':0.}
    state['manifest_sha256']=hashlib.sha256(Path(__file__).with_name('PresentationManifest.json').read_bytes()).hexdigest()
    state['system_sha256']=hashlib.sha256((ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight.uasset').read_bytes()).hexdigest()
    content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/SpellProjectiles'
    files=[p for sub in ('Materials/Fireball','Textures/Fireball','Meshes/Fireball') for p in (content/sub).glob('*.uasset')]
    state['artifact_hashes']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    assert state['engine_cap']==0
    def save():
        temp=out/'Capture.pending.json';temp.write_text(json.dumps({k:v for k,v in state.items() if k!='handle'},indent=2),'utf-8');temp.replace(out/'Capture.json')
    def finish(error=None):
        if state['handle'] is not None:u.unregister_slate_post_tick_callback(state['handle'])
        state['system_active_at_finish']=u.SystemLibrary.is_valid(comp) and comp.is_active()
        for a in (camera,ball,floor):sub.destroy_actor(a)
        perf.set_editor_property('bThrottleCPUWhenNotForeground',throttle);u.SystemLibrary.execute_console_command(world,'fx.Niagara.QualityLevel '+str(oldq));u.SystemLibrary.execute_console_command(world,'fx.EnableNiagaraLightRendering '+str(oldlight))
        state['status']='Failed' if error else 'CompletePendingHumanReview'
        if error:state['error']=str(error)
        save()
    def tick(delta):
        try:
            dt=min(float(delta),.05);state['elapsed']+=dt
            heading=0. if trajectory!='turn' else max(0.,min(math.pi/2,(state['elapsed']-.8)*math.pi/.4))
            if not state.get('deactivated'):
                state['x']+=speed*math.cos(heading)*dt;state['y']+=speed*math.sin(heading)*dt
                shown_x=math.floor(state['elapsed']*60.)/60.*speed if trajectory=='network-step' else state['x']
                ball.set_actor_location(u.Vector(shown_x,state['y'],100),False,False);ball.set_actor_rotation(u.Rotator(0,math.degrees(heading),0),False)
            x,y=state['x'],state['y'];comp.set_paused(False);comp.advance_simulation(1,dt);comp.set_paused(True)
            floor.set_actor_location(u.Vector(x-200,y,0),False,False)
            # 镜头覆盖当前合同尾长，不用旧400cm构图裁掉已加长的尾端。
            center_x=x-CONFIG['trail_length_cm']*.5
            loc=u.Vector(center_x,y+700,330);camera.set_actor_location(loc,False,False);camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(loc,u.Vector(center_x,y,100)),False)
            if state['elapsed']<1.:return
            if len(state['frames'])==0:
                comp.set_paused(False)
                cache=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(comp)
                cache=u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(cache,u.NiagaraSimCacheCreateParameters(),comp,True,.001)
                comp.set_paused(True)
                assert cache,'Native SimCache capture failed; active='+str(comp.is_active())
                pos=cache.read_position_attribute('Position','FireTrail',False)
                ids=cache.read_id_attribute('RibbonID','Filaments')
                widths=list(cache.read_float_attribute('RibbonWidth','FireTrail'))
                links=list(cache.read_float_attribute('RibbonLinkOrder','FireTrail'));ordered=[v for _,v in sorted(zip(links,pos),key=lambda pair:pair[0])]
                distance=sum(math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2) for a,b in zip(ordered,ordered[1:]))
                state['simulation']={'trail_count':len(pos),'trail_length':distance,'trail_extent_xyz':[max([getattr(p,k) for p in pos],default=0)-min([getattr(p,k) for p in pos],default=0) for k in ('x','y','z')],'filament_ids':sorted(set(v.get_editor_property('Index') for v in ids)),
                    'core_scale':[[v.x,v.y,v.z] for v in cache.read_vector_attribute('Scale','Core')],'light_radius':list(cache.read_float_attribute('LightRadius','Glow')),
                    'trail_width_min_max':[min(widths,default=0),max(widths,default=0)],'heat_alpha':[v.a for v in cache.read_color_attribute('Color','AirRipple')],
                    'glow_diameter':[[v.x,v.y] for v in cache.read_vector2_attribute('SpriteSize','Glow')],'embers':len(cache.read_position_attribute('Position','Embers',False)),
                    'engine_quality':u.SystemLibrary.get_console_variable_int_value('fx.Niagara.QualityLevel')}
            if drain and len(state['frames'])==3:
                comp.deactivate();state['deactivated']=state['elapsed']
            if drain and state.get('deactivated') and len(state['frames'])==4:
                comp.set_paused(False);after=u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(comp);after=u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(after,u.NiagaraSimCacheCreateParameters(),comp,True,.001);comp.set_paused(True)
                state['drain_initial']={'core':len(after.read_vector_attribute('Scale','Core')) if after else 0,'trail':len(after.read_position_attribute('Position','FireTrail',False)) if after else 0}
            cc.capture_scene();name='Frame%03d.png'%len(state['frames']);u.RenderingLibrary.export_render_target(world,rt,str(out),name);state['frames'].append(str(out/name));state['frame_times'].append(state['elapsed']);save()
            if len(state['frames'])>=frame_count:finish()
        except Exception as e:finish(e)
    state['handle']=u.register_slate_post_tick_callback(tick);save();print('FLIGHT_REVIEW_STARTED',str(out/'Capture.json'));return str(out/'Capture.json')


def capture(frame_count=30):
    """兼容旧调用，统一委托当前唯一飞行采样器。"""
    return capture_flight_review(frame_count=frame_count)

if __name__=="__main__":capture()
