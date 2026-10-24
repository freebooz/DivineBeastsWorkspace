# -*- coding: utf-8 -*-
"""SSLM P1 01/02 两张双掌法术真实截图：独立 UE5.8 PIE 原生渲染，不触及正式资产。
运行 -ExecCmds=py 加载；通过 -P1ScreenshotStage=01|02 确定唯一阶段。两个原生Niagara
分别摆到 Manny hand_l 和 hand_r 骨骼位置。保留真实结果和失败日志；自动退出自有编辑器。
"""
import unreal,json,os,time,traceback,hashlib
from pathlib import Path
from datetime import datetime,timezone
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
PREFIX="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/"
PATHS={"01":PREFIX+"NS_DBA_Fireball_P1_01_HandsIgnite_Review",
       "02":PREFIX+"NS_DBA_Fireball_P1_02_PalmsCharge_Review"}
CMD=unreal.SystemLibrary.get_command_line()
STAGE="02" if "-P1ScreenshotStage=02" in CMD else "01"
if "-P1UseOriginal" in CMD:
    PATHS[STAGE]="/DBAPresentationPack_Core/VFX/Library/Game/FireMage/Niagara/NS_PY_Charge"
if "-P1UseImported" in CMD:
    PATHS[STAGE]="/DBAPresentationPack_Core/VFX/Library/01_Cast/NS_Fire_Fireball_Charge"
SCALE={"01":0.65,"02":0.95}[STAGE]
RUN=ROOT/"Saved/Validation/FireballV3P1/FinalScreenshots"/(STAGE+"_"+datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
class Capture:
    def __init__(self):
        assert "-FireballP1ScreenshotOwned" in CMD
        assert "DivineBeastsArena.uproject" in unreal.Paths.get_project_file_path()
        assert unreal.SystemLibrary.get_engine_version().startswith("5.8")
        assert not (unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+
                    unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        self.asset=unreal.load_asset(PATHS[STAGE])
        assert isinstance(self.asset,unreal.NiagaraSystem)
        RUN.mkdir(parents=True,exist_ok=False)
        self.data=dict(status="started",stage=STAGE,asset=PATHS[STAGE],capture="UnrealEngine5.8 NativePIE",
                       visualsVerified=False,sourceAssetModified=False,authorityModified=False,
                       manualAccepted=False,frames=[],errors=[])
        self.start=time.monotonic()
        self.state="wait_pie";self.last=time.monotonic();self.frame=0
        self.callback=None
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        def spawn(cls,label,pos):
            a=actors.spawn_actor_from_class(cls,unreal.Vector(*pos))
            if a is None:raise RuntimeError("Failed spawn "+label)
            a.set_actor_label(label);return a
        floor=spawn(unreal.StaticMeshActor,"P1Proof_Floor",(0,0,-5))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        floor.set_actor_scale3d(unreal.Vector(20,20,1))
        key=spawn(unreal.DirectionalLight,"P1Proof_Key",(0,0,500))
        key.set_actor_rotation(unreal.Rotator(pitch=-44,yaw=-31,roll=0),False)
        key.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity",7.)
        fill=spawn(unreal.DirectionalLight,"P1Proof_Fill",(0,0,500))
        fill.set_actor_rotation(unreal.Rotator(pitch=-32,yaw=100,roll=0),False)
        fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity",2.5)
        hero=spawn(unreal.SkeletalMeshActor,"P1Proof_Caster",(0,0,0))
        hero.set_actor_rotation(unreal.Rotator(pitch=0,yaw=145,roll=0),False)
        mesh=hero.get_component_by_class(unreal.SkeletalMeshComponent)
        mesh.set_skeletal_mesh_asset(unreal.load_asset("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple"))
        seq=unreal.load_asset("/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast")
        mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        mesh.set_animation(seq)
        mesh.set_position(.28,False)
        for h in ("hand_l","hand_r"):
            assert mesh.get_bone_index(h)>=0
        for h in ("hand_l","hand_r"):
            fx=spawn(unreal.NiagaraActor,"P1Proof_Flame_"+h,(0,0,80))
            fx.set_actor_scale3d(unreal.Vector(SCALE,SCALE,SCALE))
            nc=fx.get_component_by_class(unreal.NiagaraComponent)
            nc.set_asset(self.asset);nc.set_editor_property("allow_scalability",False)
        pos=unreal.Vector(390,-515,263);look=unreal.Vector(0,0,117)
        camera=spawn(unreal.SceneCapture2D,"P1Proof_Camera",(pos.x,pos.y,pos.z))
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(pos,look),False)
        comp=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        comp.set_editor_property("capture_every_frame",False)
        comp.set_editor_property("capture_on_movement",False)
        comp.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        comp.set_editor_property("fov_angle",48.)
        pp=comp.get_editor_property("post_process_settings")
        for k,v in {"override_auto_exposure_method":True,
                    "auto_exposure_method":unreal.AutoExposureMethod.AEM_MANUAL,
                    "override_auto_exposure_bias":True,"auto_exposure_bias":0.7,
                    "override_auto_exposure_apply_physical_camera_exposure":True,
                    "auto_exposure_apply_physical_camera_exposure":False,
                    "override_motion_blur_amount":True,"motion_blur_amount":0.,
                    "override_bloom_intensity":True,"bloom_intensity":0.22}.items():
            pp.set_editor_property(k,v)
        comp.set_editor_property("post_process_settings",pp)
        self.world.get_world_settings().set_editor_property("default_game_mode",unreal.GameModeBase)
        self.callback=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        self.levels.editor_request_begin_play()
        print("P1_SCREENSHOT_STARTED",STAGE,str(RUN),flush=True)
    def persist(self):
        (RUN/"Result.json").write_text(json.dumps(self.data,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    def finish(self,status):
        if self.state in ("stopping","done"):return
        self.data["status"]=status;self.persist()
        print("P1_SCREENSHOT_FINISH",STAGE,status,str(RUN),flush=True)
        self.state="stopping";self.last=time.monotonic()
        try:
            pies=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if pies and all(w.get_name()=="Untitled" for w in pies):self.levels.editor_request_end_play()
        except Exception as e:self.data["errors"].append(str(e));self.persist()
    def tick(self,dt):
        try:
            if time.monotonic()-self.start>120:raise TimeoutError("120-second P1 proof bound")
            if self.state=="wait_pie":
                ps=unreal.EditorLevelLibrary.get_pie_worlds(False)
                if not ps:return
                assert len(ps)==1 and ps[0].get_name()=="Untitled"
                self.pie=ps[0]
                ga=unreal.GameplayStatics.get_all_actors_of_class
                heroes=ga(self.pie,unreal.SkeletalMeshActor)
                fs=ga(self.pie,unreal.NiagaraActor)
                cameras=ga(self.pie,unreal.SceneCapture2D)
                assert len(heroes)==1 and len(fs)==2 and len(cameras)==1
                self.mesh=heroes[0].get_component_by_class(unreal.SkeletalMeshComponent)
                self.mesh.set_position(.28,False)
                self.fx={}
                for f in fs:
                    name=f.get_actor_label()
                    hand="hand_l" if name.endswith("hand_l") else "hand_r" if name.endswith("hand_r") else None
                    assert hand is not None
                    c=f.get_component_by_class(unreal.NiagaraComponent)
                    self.fx[hand]=c
                    c.set_force_solo(True)
                    pos=self.mesh.get_socket_location(hand)
                    f.set_actor_scale3d(unreal.Vector(SCALE,SCALE,SCALE))
                    # 现有FireMage模板自带X+65 Z+135的局部火焰起点偏置
                    f.set_actor_location(pos-unreal.Vector(65*SCALE,0,135*SCALE),False,False)
                    c.reinitialize_system();c.activate(True)
                self.data["initialPositions"]={h:str(self.mesh.get_socket_location(h)) for h in self.fx}
                self.data["activeAtStart"]={h:bool(c.is_active()) for h,c in self.fx.items()}
                capture=cameras[0].get_component_by_class(unreal.SceneCaptureComponent2D)
                self.capture=capture
                self.target=unreal.RenderingLibrary.create_render_target2d(self.pie,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
                capture.set_editor_property("texture_target",self.target)
                self.state="warm";self.last=time.monotonic();self.persist();return
            if self.state=="warm":
                if time.monotonic()-self.last<.18:return
                for hand,c in self.fx.items():
                    if not c.is_active():raise RuntimeError("Niagara inactive "+hand)
                self.state="step";return
            if self.state=="step":
                if self.frame>=25:
                    self.finish("captured");return
                self.frame+=1
                for h,c in self.fx.items():
                    loc=self.mesh.get_socket_location(h)
                    c.get_owner().set_actor_location(loc-unreal.Vector(65*SCALE,0,135*SCALE),False,False)
                    c.advance_simulation(1,1./60.)
                if self.frame not in (9,21):return
                self.capture.capture_scene();self.state="await_capture";self.last=time.monotonic();return
            if self.state=="await_capture":
                if time.monotonic()-self.last<.17:return
                self.capture.capture_scene()
                self.state="export";self.last=time.monotonic();return
            if self.state=="export":
                if time.monotonic()-self.last<.15:return
                label="01_双手初燃" if STAGE=="01" else "02_双掌蓄力"
                path=RUN/(label+"_UE58_帧"+str(self.frame)+".png")
                unreal.RenderingLibrary.export_render_target(self.pie,self.target,str(RUN),path.name)
                if not path.exists() or path.stat().st_size<10000:
                    raise RuntimeError("native png missing "+str(path))
                self.data["frames"].append(dict(filename=path.name,frame=self.frame,bytes=path.stat().st_size,
                                                niagaraActive={h:c.is_active() for h,c in self.fx.items()}))
                self.persist()
                self.state="step";return
            if self.state=="stopping":
                if time.monotonic()-self.last>1.3:
                    self.state="done"
                    if self.callback:unreal.unregister_slate_post_tick_callback(self.callback);self.callback=None
                    unreal.SystemLibrary.quit_editor()
        except Exception:
            self.data["errors"].append(traceback.format_exc()[-5000:])
            self.persist();self.finish("failed")
            self.last=time.monotonic()
try:unreal._p1_screenshot_proof=Capture()
except Exception:
    RUN.mkdir(parents=True,exist_ok=True)
    (RUN/"BootFailed.txt").write_text(traceback.format_exc(),encoding="utf8")
    unreal.SystemLibrary.quit_editor()
