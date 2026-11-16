# -*- coding: utf-8 -*-
"""《神兽联盟》火球 V3 P1 独立 UE5.8 图形审核。由 -ExecCmds=py 启动，严格只使用临时审核地图。
真实 PIE GPU Niagara、手部代理定位、原生SceneCapture 1280x720 四阶段+逐帧证据。
不保存地图、不修改正式 GAS/服务器/客户端表现目录，自动结束自己启动的编辑器。
"""
import unreal
import json
import time
import os
import traceback
import hashlib
import shutil
from pathlib import Path
from datetime import datetime,timezone

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SYS="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review"
RUN=ROOT/"Saved/Validation/FireballV3P1/NativeRender"/datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}
CLASS="SSLM_P1_GPUReview"
sys_disk=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review.uasset"
class NativeReview:
    def __init__(self):
        argv=unreal.SystemLibrary.get_command_line()
        assert "-FireballV3CaptureOwned" in argv, "必须使用独占审核引擎"
        assert unreal.SystemLibrary.get_engine_version().startswith("5.8")
        assert unreal.Paths.get_project_file_path().replace("\\","/").endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject")
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        assert not dirty,"已有未保存包，不能切换审核世界"
        self.asset=unreal.load_asset(SYS)
        assert isinstance(self.asset,unreal.NiagaraSystem)
        RUN.mkdir(parents=True,exist_ok=False)
        self.report=dict(stage="P1",status="starting",type="Unreal Engine 5.8 native PIE GPU review",sourceNiagara=SYS,
                         previewUsesProxyHands=True,productionAbilityBound=False,productionTimingsChanged=False,
                         gameplayNetworkVerified=False,manualAccepted=False,frames=[],errors=[],
                         refImage="用户提供的火焰示例仅用于视觉参照，不进入引擎渲染画面",
                         resolution=[1280,720],sampleFps=60,shaderCompileEvidence="pending")
        self.report["systemBinarySha256"]=hashlib.sha256(sys_disk.read_bytes()).hexdigest()
        self.started=time.monotonic()
        self.handle=None
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        def spawn(cls,name,pos):
            a=self.editor.spawn_actor_from_class(cls,unreal.Vector(*pos))
            if not a:raise RuntimeError("生成审核Actor失败 "+name)
            a.set_actor_label(name);return a
        # 临时审核环境：不会写入任何 .umap 或非 P1 的持久资源。
        ground=spawn(unreal.StaticMeshActor,"SSLM_P1_Review_Stone",(0,0,-5))
        ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        ground.set_actor_scale3d(unreal.Vector(24,24,1))
        key=spawn(unreal.DirectionalLight,"SSLM_P1_KeyLight",(0,0,430))
        key.set_actor_rotation(unreal.Rotator(-48,-38,0),False)
        key.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity",6.0)
        fill=spawn(unreal.DirectionalLight,"SSLM_P1_FillLight",(0,0,430))
        fill.set_actor_rotation(unreal.Rotator(-28,110,0),False)
        fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property("intensity",2.5)
        sky=spawn(unreal.SkyLight,"SSLM_P1_SkyLight",(0,0,320))
        light=sky.get_component_by_class(unreal.SkyLightComponent)
        light.set_editor_property("source_type",unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        cube=unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")
        if cube:light.set_editor_property("cubemap",cube)
        light.set_editor_property("intensity",2.0)
        light.recapture_sky()
        mannequin=spawn(unreal.SkeletalMeshActor,"SSLM_P1_Caster",(0,0,0))
        mesh=mannequin.get_component_by_class(unreal.SkeletalMeshComponent)
        mesh.set_skeletal_mesh_asset(unreal.load_asset("/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple"))
        seq=unreal.load_asset("/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast")
        assert isinstance(seq,unreal.AnimSequence)
        mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        mesh.set_animation(seq)
        mesh.set_position(0,False)
        vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(0,0,145))
        comp=vfx.get_component_by_class(unreal.NiagaraComponent)
        comp.set_asset(self.asset)
        comp.set_editor_property("allow_scalability",False)
        comp.set_force_solo(True)
        camPos=unreal.Vector(340,-475,250);look=unreal.Vector(29,0,123)
        actor=spawn(unreal.SceneCapture2D,"SSLM_P1_GpuCapture",(camPos.x,camPos.y,camPos.z))
        actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camPos,look),False)
        capture=actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        capture.set_editor_property("capture_every_frame",False)
        capture.set_editor_property("capture_on_movement",False)
        capture.set_editor_property("capture_source",unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        capture.set_editor_property("fov_angle",54.0)
        pp=capture.get_editor_property("post_process_settings")
        for k,v in {
            "override_auto_exposure_method":True,"auto_exposure_method":unreal.AutoExposureMethod.AEM_MANUAL,
            "override_auto_exposure_bias":True,"auto_exposure_bias":0.7,
            "override_auto_exposure_apply_physical_camera_exposure":True,"auto_exposure_apply_physical_camera_exposure":False,
            "override_motion_blur_amount":True,"motion_blur_amount":0.0,
            "override_bloom_intensity":True,"bloom_intensity":0.22
        }.items():
            pp.set_editor_property(k,v)
        capture.set_editor_property("post_process_settings",pp)
        self.world.get_world_settings().set_editor_property("default_game_mode",unreal.GameModeBase)
        self.state="wait_pie"
        self.frames=0
        self.max_frames=46
        self.index=0
        self.stateStart=time.monotonic()
        self.render_target=None
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        self.levels.editor_request_begin_play()
        print("SSLM_P1_GPU_REVIEW_REQUESTED",str(RUN),flush=True)

    def persist(self):
        (RUN/"Result.json").write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding="utf-8")

    def finish(self,status):
        if self.state in ("done","quitting"):return
        self.report["status"]=status
        self.report["elapsedSeconds"]=time.monotonic()-self.started
        self.persist()
        print("SSLM_P1_GPU_REVIEW_RESULT",status,str(RUN),flush=True)
        self.state="quitting"
        self.stateStart=time.monotonic()
        try:
            pies=unreal.EditorLevelLibrary.get_pie_worlds(False)
            if pies and all(x.get_name()=="Untitled" for x in pies):
                self.levels.editor_request_end_play()
        except Exception as e:
            self.report["errors"].append("PIE结束: "+str(e))
            self.persist()

    def tick(self,delta):
        try:
            if time.monotonic()-self.started>180:raise RuntimeError("PIE GPU审核超时")
            if self.state=="wait_pie":
                worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
                if not worlds:return
                if len(worlds)!=1 or worlds[0].get_name()!="Untitled":
                    raise RuntimeError("没有获得独占的临时PIE世界")
                w=worlds[0]
                self.pie_world=w
                actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.NiagaraActor)
                meshes=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SkeletalMeshActor)
                cams=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.SceneCapture2D)
                if not (len(actors)==len(meshes)==len(cams)==1):
                    raise RuntimeError("PIE审核Actor数量不正确")
                self.comp=actors[0].get_component_by_class(unreal.NiagaraComponent)
                self.mannequin=meshes[0].get_component_by_class(unreal.SkeletalMeshComponent)
                self.camera=cams[0].get_component_by_class(unreal.SceneCaptureComponent2D)
                self.render_target=unreal.RenderingLibrary.create_render_target2d(w,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
                self.camera.set_editor_property("texture_target",self.render_target)
                self.comp.set_editor_property("allow_scalability",False)
                self.comp.set_force_solo(True)
                self.comp.reinitialize_system()
                self.comp.activate(True)
                self.report["activeAtInit"]=bool(self.comp.is_active())
                self.comp.set_component_tick_enabled(False)
                self.report["status"]="capturing"
                self.state="warm"
                self.stateStart=time.monotonic()
                self.persist()
                return
            if self.state=="warm":
                if time.monotonic()-self.stateStart<.35:return
                if not self.comp.is_active():
                    raise RuntimeError("Niagara系统启动失败：GPU编译或生命周期未通过")
                self.state="advance"
                return
            if self.state=="advance":
                if self.frames>=self.max_frames:
                    self.finish("captured")
                    return
                self.comp.advance_simulation(1,1/60)
                self.frames+=1
                self.mannequin.set_position(min(self.frames/60,0.70),False)
                if not self.comp.is_active() and self.frames<35:
                    raise RuntimeError("Niagara在蓄力结束前异常退出，帧 "+str(self.frames))
                self.camera.capture_scene()
                self.state="capture2"
                self.stateStart=time.monotonic()
                return
            if self.state=="capture2":
                if time.monotonic()-self.stateStart<.06:return
                self.camera.capture_scene()
                self.state="export"
                self.stateStart=time.monotonic()
                return
            if self.state=="export":
                if time.monotonic()-self.stateStart<.07:return
                filename="Frame_"+str(self.frames).zfill(3)+".png"
                unreal.RenderingLibrary.export_render_target(self.pie_world,self.render_target,str(RUN),filename)
                path=RUN/filename
                if not path.exists() or path.stat().st_size<4000:
                    raise RuntimeError("原生图形帧未能导出："+filename)
                key=STAGES.get(self.frames)
                if key:shutil.copy2(path,RUN/(key+".png"))
                self.report["frames"].append(dict(index=self.frames,ageSeconds=round(self.frames/60,4),
                                                  bytes=path.stat().st_size,filename=filename,keyStage=key))
                self.persist()
                self.state="advance"
                return
            if self.state=="quitting":
                if time.monotonic()-self.stateStart>1.0:
                    self.state="done"
                    if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
                    unreal.SystemLibrary.quit_editor()
        except Exception:
            self.report["errors"].append(traceback.format_exc()[-4500:])
            self.persist()
            self.finish("failed")
            self.stateStart=time.monotonic()

try:
    unreal._ssml_native_p1=NativeReview()
except Exception:
    RUN.mkdir(parents=True,exist_ok=True)
    (RUN/"BootFailure.txt").write_text(traceback.format_exc(),encoding="utf-8")
    print("SSLM_P1_BOOT_FAILURE",traceback.format_exc()[-1700:],flush=True)
    unreal.SystemLibrary.quit_editor()
