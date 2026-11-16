# -*- coding: utf-8 -*-
"""《神兽联盟》火球V3 P1独占UE5.8原生审查：双手前摇，逐帧GPU捕获并导出PNG。
仅创建瞬态未保存的审核世界/Actor，不更改游戏地图或Gameplay；保留Niagara原生渲染证据与完整采样信息。
需要用Monolith在已确认属于SSLM的编辑器运行本文件；结束时注销Slate回调，等待人工验收。
"""
import unreal
import json
import shutil
import time
import traceback
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
ASSET = '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review'
RUN = ROOT / 'Saved/Validation/FireballV3P1' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
STAGES = {7:'01_HandsIgnite', 15:'02_PalmsCharge', 29:'03_StreamConverge', 41:'04_CoreFormed'}

class FireballP1NativeReview:
    def __init__(self):
        assert 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
        assert unreal.load_asset(ASSET) is not None, '原生Niagara审核资产不存在'
        dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        assert not dirty, '编辑器存在未保存资产，拒绝切换到瞬态审核世界'
        if getattr(unreal, '_ssml_fireball_p1_review', None) is not None:
            old = unreal._ssml_fireball_p1_review
            if old.handle:
                raise RuntimeError('之前的采样仍在运行')
        RUN.mkdir(parents=True, exist_ok=False)
        self.report = {
            'status':'running','source':'UE5.8 原生Niagara + SceneCapture FinalColorLDR',
            'niagaraAsset':ASSET,'manualVisualAccepted':False,'actualGameRuntimeVerified':False,
            'productionBindingChanged':False,'frames':[],'nativeScreenshot':True,
            'resolution':[1280,720],'frameRate':60,'errors':[]
        }
        self.handle=None
        self.started=time.monotonic()
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        def spawn(cls,label,pos):
            act=self.actors.spawn_actor_from_class(cls,unreal.Vector(*pos))
            if not act:raise RuntimeError('无法生成审核临时Actor '+label)
            act.set_actor_label(label)
            return act
        self.floor=spawn(unreal.StaticMeshActor,'SSLM_P1_Review_Floor',(0,0,-8))
        self.floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        self.floor.set_actor_scale3d(unreal.Vector(18,18,1))
        self.light=spawn(unreal.DirectionalLight,'SSLM_P1_Review_Key',(200,0,450))
        self.light.set_actor_rotation(unreal.Rotator(-40,-40,0),False)
        self.light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',3.8)
        self.fill=spawn(unreal.DirectionalLight,'SSLM_P1_Review_Fill',(0,200,330))
        self.fill.set_actor_rotation(unreal.Rotator(-33,100,0),False)
        self.fill.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',1.1)
        self.model=spawn(unreal.SkeletalMeshActor,'SSLM_P1_Casting_Mannequin',(0,0,0))
        self.mesh=self.model.get_component_by_class(unreal.SkeletalMeshComponent)
        self.mesh.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
        seq=unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/DevelopmentCasting/AS_DBA_DevFireballCast')
        assert isinstance(seq,unreal.AnimSequence)
        self.mesh.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        self.mesh.set_animation(seq)
        self.mesh.set_position(0,False)
        self.vfx=spawn(unreal.NiagaraActor,'SSLM_P1_CastV3',(0,0,147))
        self.comp=self.vfx.get_component_by_class(unreal.NiagaraComponent)
        self.comp.set_asset(unreal.load_asset(ASSET))
        self.comp.set_editor_property('allow_scalability',False)
        self.comp.set_force_solo(True)
        self.comp.reinitialize_system()
        self.comp.set_component_tick_enabled(False)
        eye=unreal.Vector(300,-415,243)
        aim=unreal.Vector(29,0,118)
        self.camera=spawn(unreal.SceneCapture2D,'SSLM_P1_NativeCapture',(eye.x,eye.y,eye.z))
        self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye,aim),False)
        self.capture=self.camera.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.capture.set_editor_property('capture_every_frame',False)
        self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('fov_angle',51)
        pp=self.capture.get_editor_property('post_process_settings')
        for name,val in {
            'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,
            'override_auto_exposure_bias':True,'auto_exposure_bias':0.0,
            'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,
            'override_motion_blur_amount':True,'motion_blur_amount':0.0,
            'override_bloom_intensity':True,'bloom_intensity':0.22
        }.items():
            pp.set_editor_property(name,val)
        self.capture.set_editor_property('post_process_settings',pp)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        self.capture.set_editor_property('texture_target',self.target)
        try:
            self.levels.editor_set_viewport_realtime(True)
            self.levels.editor_set_game_view(True)
        except Exception:
            pass
        self.samples=list(range(1,45,2))
        self.count=0
        self.index=0
        self.warm=0
        self.phase='warm'
        self.since=self.started
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        print('SSLM_P1_UE58_NATIVE_CAPTURE_STARTED',str(RUN))

    def persist(self):
        (RUN/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')

    def finish(self,status):
        if self.handle:
            unreal.unregister_slate_post_tick_callback(self.handle)
            self.handle=None
        self.report['status']=status
        self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist()
        print('SSLM_P1_UE58_NATIVE_CAPTURE_FINISHED',status,str(RUN))

    def tick(self,delta):
        try:
            if time.monotonic()-self.started>150:
                raise TimeoutError('P1原生Niagara审核截图超时')
            if self.phase=='warm':
                self.warm+=1
                if self.warm<35:return
                self.phase='advance'
            if self.phase=='advance':
                if self.count<self.samples[self.index]:
                    self.comp.advance_simulation(1,1/60)
                    self.count+=1
                    self.mesh.set_position(min(self.count/60,0.68),False)
                    return
                self.capture.capture_scene()
                self.phase='capture2'
                self.since=time.monotonic()
                return
            if time.monotonic()-self.since<0.14:return
            if self.phase=='capture2':
                self.capture.capture_scene()
                self.phase='export'
                self.since=time.monotonic()
                return
            filename='Frame_'+str(self.index).zfill(3)+'.png'
            unreal.RenderingLibrary.export_render_target(self.world,self.target,str(RUN),filename)
            path=RUN/filename
            if not path.is_file() or path.stat().st_size<=3000:
                raise RuntimeError('真实帧导出失败 '+filename)
            stage=STAGES.get(self.count)
            if stage:
                shutil.copyfile(path,RUN/(stage+'.png'))
            self.report['frames'].append({'index':self.index,'simulationTimeSeconds':round(self.count/60,4),'path':str(path),'bytes':path.stat().st_size,'keyStage':stage})
            self.persist()
            self.index+=1
            if self.index==len(self.samples):
                self.finish('captured')
            else:
                self.phase='advance'
        except Exception:
            self.report['errors'].append(traceback.format_exc())
            self.finish('failed')

unreal._ssml_fireball_p1_review=FireballP1NativeReview()
