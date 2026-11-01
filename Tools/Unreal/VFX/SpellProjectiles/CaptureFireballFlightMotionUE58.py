# -*- coding: utf-8 -*-
"""Monolith原生移动RHI采样；使用真实编辑器帧，清理全部临时Actor，不保存地图。

固定3000cm/s仅为表现夹具；30fps为媒体采样，不限制游戏或编辑器帧率。
"""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/FireballTrail20261101/NativeMotion'

def capture(frame_count=30):
    """异步原生帧后采样，给Niagara的RenderProxy真实帧更新机会，返回捕获状态文件。"""
    OUT.mkdir(parents=True,exist_ok=True)
    assert '-SSLMFireballMaterialsOwned' in u.SystemLibrary.get_command_line()
    sub=u.get_editor_subsystem(u.EditorActorSubsystem)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    ball=sub.spawn_actor_from_class(u.NiagaraActor,u.Vector(-1200,0,220),transient=True)
    camera=sub.spawn_actor_from_class(u.SceneCapture2D,u.Vector(900,2800,600),transient=True)
    component=ball.get_editor_property('niagara_component');component.set_force_solo(True)
    component.set_asset(u.load_asset('/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight'))
    component.activate(True);component.set_paused(True)
    cc=camera.get_component_by_class(u.SceneCaptureComponent2D)
    target=u.RenderingLibrary.create_render_target2d(world,1280,720,u.TextureRenderTargetFormat.RTF_RGBA8)
    cc.set_editor_property('texture_target',target)
    cc.set_editor_property('capture_every_frame',False);cc.set_editor_property('capture_on_movement',False)
    cc.set_editor_property('capture_source',u.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    cc.set_editor_property('primitive_render_mode',u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    assert cc.get_owner()==camera
    cc.show_only_actor_components(ball);cc.set_editor_property('fov_angle',60.0)
    state={'Status':'Capturing','FrameCount':frame_count,'Frames':[],'Warmup':0,'Step':0,'Handle':None,
           'MediaSampleRate':30,'SpeedCmPerSecond':3000,'AuthorityMovementVerified':False,'EngineFrameCapChanged':False}
    def save():
        result={k:v for k,v in state.items() if k!='Handle'}
        (OUT/'Capture.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    def finish(error=None):
        if state['Handle'] is not None:u.unregister_slate_post_tick_callback(state['Handle'])
        if error:state['Error']=str(error)
        state['Status']='Failed' if error else 'CapturedPendingPixelReview'
        sub.destroy_actor(camera);sub.destroy_actor(ball);save()
    def tick(delta):
        try:
            component.set_paused(False)
            for _ in range(2):
                state['Step']+=1
                ball.set_actor_location(u.Vector(-1200+state['Step']*50,0,220),False,False)
                component.advance_simulation(1,1/60)
            component.set_paused(True)
            center=ball.get_actor_location();view=u.Vector(center.x-900,2800,600)
            camera.set_actor_location(view,False,False)
            camera.set_actor_rotation(u.MathLibrary.find_look_at_rotation(view,u.Vector(center.x-900,0,220)),False)
            state['Warmup']+=1
            if state['Warmup']<=32:return
            cc.capture_scene()
            name='Frame%03d.png'%len(state['Frames'])
            u.RenderingLibrary.export_render_target(world,target,str(OUT),name)
            path=OUT/name
            assert path.exists() and path.stat().st_size>1000,'未导出原生RHI帧'
            state['Frames'].append(str(path));save()
            if len(state['Frames'])>=frame_count:finish()
        except Exception as error:finish(error)
    state['Handle']=u.register_slate_post_tick_callback(tick)
    save();print('NATIVE_CAPTURE_ASYNC_STARTED',frame_count)
    return str(OUT/'Capture.json')

if __name__=='__main__':capture()
