# -*- coding: utf-8 -*-
"""SSLM 火球 V3 P1 01/02 双手原生预览：用 UE5.8 独占编辑器和真实 PIE GPU/CPU Niagara 录帧。

左右手在每个游戏审核帧读 Manny hand_l / hand_r 骨骼位置，两个 Niagara 实例分别跟随。
现有 NS_PY_Charge 内部火焰中心偏置大致 (+65,0,+135) cm，本测试用代理校正到掌心；
这是审查定位方案，不替代正式项目层骨骼 Socket 数据。
Stage01: 低能量掌火；Stage02: 大体积双掌聚火，两阶段均不修改服务器权威施法时序。
所有关卡与 Actor 瞬态，不保存真实地图；结束后停止独占的 UE5.8 编辑器。
"""
from pathlib import Path
import unreal
import json
import time
import hashlib
import traceback
import os
import shutil
from datetime import datetime,timezone

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
RBASE='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara'
ASSETS={
 '01':RBASE+'/NS_DBA_Fireball_P1_01_HandsIgnite_Review',
 '02':RBASE+'/NS_DBA_Fireball_P1_02_PalmsCharge_Review'
}
FOLDER=ROOT/'Saved/Validation/FireballV3P1/TwoHandNativeCapture'
RUN=FOLDER/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
SETTINGS={
 '01':{'scale':0.43,'samples':12,'keys':{4:'01_双手初燃_前期',11:'01_双手初燃_成形'}},
 '02':{'scale':0.68,'samples':27,'keys':{7:'02_双掌蓄力_前期',23:'02_双掌蓄力_增强'}}
}
class NativeP1TwoHands:
    def __init__(self):
        argv=unreal.SystemLibrary.get_command_line()
        assert '-FireballP1HandsOwned' in argv, "拒绝在其他正在使用的编辑器中执行"
        assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
        assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        assert not dirty,'未保存工作包存在，禁止建立审核世界'
        self.resources={k:unreal.load_asset(v) for k,v in ASSETS.items()}
        assert all(isinstance(v,unreal.NiagaraSystem) for v in self.resources.values()),'P1两个审核Niagara未创建'
        RUN.mkdir(parents=True,exist_ok=False)
        self.data={'project':unreal.Paths.get_project_file_path(),'editorPID':os.getpid(),
          'engine':unreal.SystemLibrary.get_engine_version(),'status':'initializing',
          'sourceNiagara':ASSETS,'artistReviewRequired':True,'productionBound':False,'authorityTimelineChanged':False,
          'sourceTexture':'T_PY_FlameFlipbook','captureSource':'UE5.8 PIE SceneCapture FinalColorLDR',
          'resolution':[1280,720],'frames':[],'errors':[],
          'handBinding':'Manny get_socket_location(hand_l,hand_r), following world-space; offset compensation of source template',
          'captureRealEngine':True,'visualApproved':False,'simulationApproved':False}
        self.started=time.monotonic()
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.state='wait_pie';self.frame=0;self.stage='01';self.stageFrame=0;self.lastWait=time.monotonic()
        self.callback=None
        def spawn(cls,label,pos):
            a=self.editor.spawn_actor_from_class(cls,unreal.Vector(*pos))
            if not a:raise RuntimeError('生成临时审查 Actor 失败 '+label)
            a.set_actor_label(label)
            return a
        floor=spawn(unreal.StaticMeshActor,'P1_Review_Stone',(0,0,-4))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        floor.set_actor_scale3d(unreal.Vector(22,22,1))
        light=spawn(unreal.DirectionalLight,'P1_Review_Daylight',(0,0,430))
        light.set_actor_rotation(unreal.Rotator(-50,-44,0),False)
        light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',7.0)
        fill=spawn(unreal.DirectionalLight,'P1_Review_SoftFill',(0,0,450))
        fill.set_actor_rotation(unreal.Rotator(-28,86,0),False)
        fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',2.3)
        sk=spawn(unreal.SkyLight,'P1_Review_Sky',(0,0,250))
        sl=sk.get_component_by_class(unreal.SkyLightComponent)
        sl.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        cb=unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
        if cb: sl.set_editor_property('cubemap',cb)
        sl.set_editor_property('intensity',1.6)
        sl.recapture_sky()
        hero=spawn(unreal.SkeletalMeshActor,'P1_Review_Caster',(0,0,0))
        hero.set_actor_rotation(unreal.Rotator(pitch=0,yaw=145,roll=0),False)
        h=hero.get_component_by_class(unreal.SkeletalMeshComponent)
        h.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
        seq=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast')
        assert isinstance(seq,unreal.AnimSequence)
        h.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        h.set_animation(seq)
        h.set_position(0,False)
        for bone in ['hand_l','hand_r']:
            index=h.get_bone_index(bone)
            if index<0:raise RuntimeError('所选骨架缺少 '+bone)
        for stage in ('01','02'):
            for hand in ('hand_l','hand_r'):
                obj=spawn(unreal.NiagaraActor,f'P1_{stage}_{hand}',(0,0,100))
                c=obj.get_component_by_class(unreal.NiagaraComponent)
                c.set_asset(self.resources[stage])
                c.set_auto_activate(False)
                c.set_editor_property('allow_scalability',False)
        camera=spawn(unreal.SceneCapture2D,'P1_Review_Camera',(360,-490,245))
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(360,-490,245),unreal.Vector(0,0,117)),False)
        self.camObj=camera
        cap=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        cap.set_editor_property('capture_every_frame',False)
        cap.set_editor_property('capture_on_movement',False)
        cap.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cap.set_editor_property('fov_angle',53.0)
        pp=cap.get_editor_property('post_process_settings')
        for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,
            'override_auto_exposure_bias':True,'auto_exposure_bias':0.65,
            'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,
            'override_motion_blur_amount':True,'motion_blur_amount':0.0,
            'override_bloom_intensity':True,'bloom_intensity':0.20}.items():
            pp.set_editor_property(k,v)
        cap.set_editor_property('post_process_settings',pp)
        self.world.get_world_settings().set_editor_property('default_game_mode',unreal.GameModeBase)
        self.callback=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        self.levels.editor_request_begin_play()
        print('P1_TWO_HANDS_UE58_STARTED',str(RUN),flush=True)

    def persist(self):
        (RUN/'Result.json').write_text(json.dumps(self.data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

    def activate_stage(self,stage):
        self.stage=stage;self.stageFrame=0
        for a in self.components.values():a.deactivate()
        for hand in ('hand_l','hand_r'):
            c=self.components[(stage,hand)]
            scale=SETTINGS[stage]['scale']
            palm=self.mesh.get_socket_location(hand)
            c.get_owner().set_actor_scale3d(unreal.Vector(scale,scale,scale))
            c.get_owner().set_actor_location(palm-unreal.Vector(65*scale,0,135*scale),False,False)
            c.set_force_solo(True)
            c.set_component_tick_enabled(True)
            c.reinitialize_system()
            c.activate(True)
            if not c.is_active():raise RuntimeError('Niagara激活失败：'+stage+hand)
            c.set_component_tick_enabled(False)
        self.data['stage']=stage
        self.persist()

    def finish(self,status):
        if self.state in ('quitting','done'):return
        self.data['status']=status
        self.data['totalElapsedSeconds']=time.monotonic()-self.started
        self.data['stagesCaptured']={k:sum(1 for v in self.data['frames'] if v['stage']==k) for k in ('01','02')}
        self.persist()
        print('P1_TWO_HANDS_UE58_CAPTURE',status,str(RUN),flush=True)
        self.state='quitting';self.lastWait=time.monotonic()
        try:
            pies=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if pies and all(w.get_name()=='Untitled' for w in pies):self.levels.editor_request_end_play()
        except Exception as e:
            self.data['errors'].append(str(e));self.persist()

    def tick(self,delta):
        try:
            if time.monotonic()-self.started>150:raise TimeoutError('独占P1审核超时')
            if self.state=='wait_pie':
                worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
                if not worlds:return
                if len(worlds)!=1 or worlds[0].get_name()!='Untitled':raise RuntimeError('当前不是独占PIE临时审核世界')
                self.pie=worlds[0]
                actor=unreal.GameplayStatics.get_all_actors_of_class
                heroes=actor(self.pie,unreal.SkeletalMeshActor)
                vfxactors=actor(self.pie,unreal.NiagaraActor)
                cams=actor(self.pie,unreal.SceneCapture2D)
                assert len(heroes)==1 and len(vfxactors)==4 and len(cams)==1
                self.mesh=heroes[0].get_component_by_class(unreal.SkeletalMeshComponent)
                self.components={}
                for o in vfxactors:
                    label=o.get_actor_label()
                    for stage in ('01','02'):
                        for hand in ('hand_l','hand_r'):
                            if label==f'P1_{stage}_{hand}':
                                self.components[(stage,hand)]=o.get_component_by_class(unreal.NiagaraComponent)
                assert len(self.components)==4
                self.cap=cams[0].get_component_by_class(unreal.SceneCaptureComponent2D)
                self.render=unreal.RenderingLibrary.create_render_target2d(self.pie,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
                self.cap.set_editor_property('texture_target',self.render)
                self.mesh.set_position(0,False)
                self.activate_stage('01')
                self.state='warm';self.lastWait=time.monotonic()
                return
            if self.state=='warm':
                if time.monotonic()-self.lastWait<0.35:return
                self.state='advance';return
            if self.state=='advance':
                settings=SETTINGS[self.stage]
                if self.stageFrame>=settings['samples']:
                    if self.stage=='01':
                        self.activate_stage('02');self.state='warm';self.lastWait=time.monotonic();return
                    self.finish('captured');return
                self.stageFrame+=1
                self.mesh.set_position(min(0.7,(self.stageFrame/60)+(0 if self.stage=='01' else .20)),False)
                hands={}
                scale=settings['scale']
                for hand in ('hand_l','hand_r'):
                    xyz=self.mesh.get_socket_location(hand)
                    hands[hand]=[round(xyz.x,2),round(xyz.y,2),round(xyz.z,2)]
                    c=self.components[(self.stage,hand)]
                    c.get_owner().set_actor_scale3d(unreal.Vector(scale,scale,scale))
                    c.get_owner().set_actor_location(xyz-unreal.Vector(65*scale,0,135*scale),False,False)
                    c.advance_simulation(1,1/60)
                self.lastHands=hands
                if self.stageFrame not in SETTINGS[self.stage]['keys']:
                    self.state='advance'
                    return
                self.cap.capture_scene()
                self.state='capture2';self.lastWait=time.monotonic()
                return
            if self.state=='capture2':
                if time.monotonic()-self.lastWait<0.06:return
                self.cap.capture_scene()
                self.state='export';self.lastWait=time.monotonic()
                return
            if self.state=='export':
                if time.monotonic()-self.lastWait<0.06:return
                filename=f'{self.stage}_Frame_{self.stageFrame:03d}.png'
                unreal.RenderingLibrary.export_render_target(self.pie,self.render,str(RUN),filename)
                pic=RUN/filename
                if not pic.exists() or pic.stat().st_size<4000:
                    if time.monotonic()-self.lastWait<3.0:return
                    raise RuntimeError('原生截图导出失败：'+filename)
                k=SETTINGS[self.stage]['keys'].get(self.stageFrame)
                if k:
                    try:
                        os.link(pic,RUN/(k+'.png'))  # NTFS同盘硬链接，不重复占用磁盘空间
                    except OSError:
                        pass  # 保留已导出的原生Frame与Result.json，关键帧别名不是通过门禁
                self.data['frames'].append({'stage':self.stage,'frame':self.stageFrame,'name':filename,
                         'simTimeSeconds':round(self.stageFrame/60,4),'size':pic.stat().st_size,
                         'key':k,'handLocations':self.lastHands})
                self.persist()
                self.state='advance';return
            if self.state=='quitting':
                if time.monotonic()-self.lastWait>1.2:
                    self.state='done'
                    if self.callback:unreal.unregister_slate_post_tick_callback(self.callback);self.callback=None
                    unreal.SystemLibrary.quit_editor()
        except Exception:
            self.data['errors'].append(traceback.format_exc()[-4500:])
            self.persist()
            self.finish('failed')
            self.lastWait=time.monotonic()

try:
    unreal._p1_twohands_review=NativeP1TwoHands()
except Exception:
    RUN.mkdir(parents=True,exist_ok=True)
    (RUN/'BootFailure.txt').write_text(traceback.format_exc(),encoding='utf-8')
    print('P1_TWO_HANDS_REVIEW_BOOT_FAILED',traceback.format_exc()[-1900:],flush=True)
    unreal.SystemLibrary.quit_editor()
