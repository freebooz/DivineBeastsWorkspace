# -*- coding: utf-8 -*-
"""SSLM V2核心材质原生检查：独立临时编辑器世界，真实网格和真实材质，不伪装成最终法术。
先验证主体三维形态与光照；用不同机位及逐次捕获哈希防止重复旧帧。
不保存地图、不改正式资产、不关闭别人的编辑器。
"""
import unreal,time,json,traceback,hashlib
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
DEST='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2'
OUT=ROOT/'Saved/Validation/OrbProjectileV2/SurfaceProof'
class SurfaceReview:
    def __init__(self):
        assert '-OrbV2ReworkOwned' in unreal.SystemLibrary.get_command_line()
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        self.sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.start=time.monotonic();self.phase='warm';self.last=self.start;self.index=0;self.handle=None
        OUT.mkdir(parents=True,exist_ok=True)
        self.report={'status':'rendering','kind':'UE5.8 native material and mesh proof, not final Niagara','humanAccepted':False,'frames':[],'errors':[]}
        def spawn(cls,pos,label):
            a=self.sub.spawn_actor_from_class(cls,unreal.Vector(*pos));a.set_actor_label(label);return a
        self.spawn=spawn
        self.fire=spawn(unreal.StaticMeshActor,(0,-72,132),'OrbV2_Proof_FireCore')
        self.fire.static_mesh_component.set_static_mesh(unreal.load_asset(DEST+'/Meshes/SM_DBA_FireCore_V2'))
        self.fire.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_DBA_FireCoreFlow_V2'))
        self.ice=spawn(unreal.StaticMeshActor,(0,76,132),'OrbV2_Proof_IceCrystal')
        self.ice.static_mesh_component.set_static_mesh(unreal.load_asset(DEST+'/Meshes/SM_DBA_IceCrystal_V2'))
        self.ice.static_mesh_component.set_material(0,unreal.load_asset(DEST+'/Materials/M_DBA_IceCrystal_V2'))
        light=spawn(unreal.DirectionalLight,(0,0,400),'OrbV2_Key')
        light.set_actor_rotation(unreal.Rotator(-38,-35,0),False)
        light.light_component.set_editor_property('intensity',5.0)
        fill=spawn(unreal.DirectionalLight,(0,0,400),'OrbV2_Fill')
        fill.set_actor_rotation(unreal.Rotator(-15,140,0),False)
        fill.light_component.set_editor_property('intensity',2.0)
        sky=spawn(unreal.SkyLight,(0,0,400),'OrbV2_Ambient')
        sc=sky.get_component_by_class(unreal.SkyLightComponent)
        sc.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sc.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        sc.set_editor_property('intensity',1.4);sc.recapture_sky()
        floor=spawn(unreal.StaticMeshActor,(0,0,0),'OrbV2_Floor')
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        floor.set_actor_scale3d(unreal.Vector(14,14,1))
        cap=spawn(unreal.SceneCapture2D,(290,-360,225),'OrbV2_SurfaceCapture')
        self.camera_actor=cap;self.capture=cap.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.capture.set_editor_property('capture_every_frame',False);self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('fov_angle',44.0)
        pp=self.capture.get_editor_property('post_process_settings')
        for k,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_auto_exposure_bias':True,'auto_exposure_bias':.0,'override_bloom_intensity':True,'bloom_intensity':.14,'override_motion_blur_amount':True,'motion_blur_amount':.0}.items():pp.set_editor_property(k,v)
        self.capture.set_editor_property('post_process_settings',pp)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1920,1080,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        self.capture.set_editor_property('texture_target',self.target)
        self.views=[('V2_CorePair_Front',(340,-305,220),(0,0,125)),('V2_FireCore_Close',(205,-310,185),(0,-72,132)),('V2_IceCrystal_Close',(210,-145,185),(0,76,132)),('V2_CorePair_Side',(340,300,230),(0,0,125))]
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
    def persist(self):(OUT/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
    def tick(self,dt):
        try:
            now=time.monotonic()
            if now-self.start>120:raise RuntimeError('真实材质审核捕获超时')
            if self.phase=='warm':
                if now-self.start<3:return
                self.phase='view'
            if self.phase=='view':
                if self.index>=len(self.views):
                    self.report['status']='captured_pending_manual_review';self.persist()
                    unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None;return
                name,pos,look=self.views[self.index]
                self.camera_actor.set_actor_location(unreal.Vector(*pos),False,False)
                self.camera_actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*pos),unreal.Vector(*look)),False)
                self.capture.capture_scene();self.last=now;self.phase='flush';return
            if self.phase=='flush':
                if now-self.last<.30:return
                self.capture.capture_scene();self.last=now;self.phase='export';return
            if self.phase=='export':
                if now-self.last<.25:return
                name,pos,look=self.views[self.index]
                path=OUT/(name+'.png')
                unreal.RenderingLibrary.export_render_target(self.world,self.target,str(OUT),path.name)
                if not path.is_file() or path.stat().st_size<4000:raise RuntimeError('原生帧导出失败 '+path.name)
                self.report['frames'].append({'file':path.name,'camera':pos,'lookAt':look,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bytes':path.stat().st_size})
                self.index+=1;self.phase='view';self.persist()
        except Exception:
            self.report['status']='failed';self.report['errors'].append(traceback.format_exc());self.persist()
            if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
unreal._orbv2_surface=SurfaceReview()
print('ORBV2_SURFACE_PROOF_STARTED',OUT)
