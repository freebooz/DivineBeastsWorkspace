# -*- coding: utf-8 -*-
"""Monolith内运行的弧刃审核场景：固定相机/曝光，有限原生渲染回调。

仅创建本样本的审核世界；每个导出帧均由UE SceneCapture直接渲染。
制作时的离散模拟时钟不是帧率测量，也不参与玩法或生产播放器。
"""
from pathlib import Path
from datetime import datetime,timezone
import json,time,traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash'


class AttackSlashReview:
    def __init__(self,clip=False):
        self.handle=None
        assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
        old=getattr(unreal,'_attack_slash_v2_review',None)
        if old and old.handle:raise RuntimeError('已有审核制作回调，拒绝重入')
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        dirty_maps=unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if dirty or dirty_maps:raise RuntimeError('UNSAVED_PACKAGES: 先处理未保存内容/地图，不替换当前世界：'+str([p.get_path_name() for p in dirty+dirty_maps]))
        self.clip=clip;self.out=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2/RenderRuns'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');self.out.mkdir(parents=True)
        self.report=dict(status='running',manualAccepted=False,renderer='UE5.8 SceneCapture FinalColorLDR',resolution=[1440,900],simulationFPS=60,encodingFPS=30,performanceMeasured=False,frames=[])
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        ground_path=BASE+'/Review/M_SlashReview_Stone'
        ground=unreal.load_asset(ground_path)
        if not ground:
            ground=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SlashReview_Stone',BASE+'/Review',unreal.Material,unreal.MaterialFactoryNew())
            lib=unreal.MaterialEditingLibrary
            pos=lib.create_material_expression(ground,unreal.MaterialExpressionWorldPosition)
            custom=lib.create_material_expression(ground,unreal.MaterialExpressionCustom)
            i=unreal.CustomInput();i.set_editor_property('input_name','P');custom.set_editor_property('inputs',[i]);custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            custom.set_editor_property('code','float2 tile=abs(frac(P.xy/115.0)-.5);float seam=smoothstep(.478,.49,max(tile.x,tile.y));return lerp(float3(.30,.285,.255),float3(.22,.215,.20),seam);')
            lib.connect_material_expressions(pos,'XYZ',custom,'P');lib.connect_material_property(custom,'',unreal.MaterialProperty.MP_BASE_COLOR)
            rough=lib.create_material_expression(ground,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.78);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
            lib.recompile_material(ground);assert unreal.EditorAssetLibrary.save_loaded_asset(ground,False)
        floor=self.spawn(unreal.StaticMeshActor,'SlashReview_Stone',(0,0,-2));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(unreal.Vector(40,40,1));floor.static_mesh_component.set_material(0,ground)
        for label,rotation,strength in [('Key',(-55,-35,0),4.5),('Fill',(-35,150,0),.75)]:
            light=self.spawn(unreal.DirectionalLight,'SlashReview_'+label,(0,0,600));light.set_actor_rotation(unreal.Rotator(pitch=rotation[0],yaw=rotation[1],roll=rotation[2]),False);light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',strength)
        sky=self.spawn(unreal.SkyLight,'SlashReview_Sky',(0,0,400)).get_component_by_class(unreal.SkyLightComponent)
        sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.set_editor_property('intensity',2.0)
        neutral=unreal.load_asset(BASE+'/Review/M_SlashReview_Neutral')
        if not neutral:
            neutral=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SlashReview_Neutral',BASE+'/Review',unreal.Material,unreal.MaterialFactoryNew())
            lib=unreal.MaterialEditingLibrary;n=lib.create_material_expression(neutral,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(.5,.49,.46,1));lib.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
            r=lib.create_material_expression(neutral,unreal.MaterialExpressionConstant);r.set_editor_property('r',.42);lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS);lib.recompile_material(neutral);assert unreal.EditorAssetLibrary.save_loaded_asset(neutral,False)
        for label,position,yaw in [('Caster',(-220,160,0),25),('Target',(350,-60,0),205)]:
            a=self.spawn(unreal.SkeletalMeshActor,'SlashReview_'+label,position);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
            sk=a.get_component_by_class(unreal.SkeletalMeshComponent);sk.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
            for slot in range(sk.get_num_materials()):sk.set_material(slot,neutral)
            sk.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);sk.set_animation(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle'));sk.set_position(.4,False)
        center=unreal.Vector(15,10,100);position=center+unreal.Vector(-150,1050,650)
        self.camera=self.spawn(unreal.CameraActor,'SlashReview_Camera',position);self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,center),False)
        camera=self.camera.get_component_by_class(unreal.CameraComponent);camera.set_field_of_view(43)
        pp=camera.get_editor_property('post_process_settings')
        for p,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_motion_blur_amount':True,'motion_blur_amount':0.,'override_bloom_intensity':True,'bloom_intensity':.32}.items():pp.set_editor_property(p,v)
        camera.set_editor_property('post_process_settings',pp)
        self.capactor=self.spawn(unreal.SceneCapture2D,'SlashReview_Capture',position);self.capactor.set_actor_transform(self.camera.get_actor_transform(),False,False)
        self.capture=self.capactor.get_component_by_class(unreal.SceneCaptureComponent2D)
        for p,v in {'capture_every_frame':False,'capture_on_movement':False,'capture_source':unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,'post_process_settings':pp,'fov_angle':43.}.items():self.capture.set_editor_property(p,v)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1440,900,unreal.TextureRenderTargetFormat.RTF_RGBA8);self.capture.set_editor_property('texture_target',self.target)
        self.actor=self.spawn(unreal.NiagaraActor,'SlashReview_Attack',(0,0,0));self.component=self.actor.get_component_by_class(unreal.NiagaraComponent)
        self.component.set_asset(unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V2'));self.component.set_editor_property('allow_scalability',False);self.component.set_force_solo(True);self.component.reinitialize_system();self.component.set_component_tick_enabled(False)
        self.levels.pilot_level_actor(self.camera);self.levels.editor_set_viewport_realtime(True);self.levels.editor_set_game_view(True);unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
        self.jobs=list(range(2,63,2)) if clip else [9,21,35,48,62]
        self.steps=0;self.index=0;self.warm=0;self.state='warm';self.since=time.monotonic();self.started=self.since
        self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist()

    def spawn(self,cls,label,position):
        a=self.actors.spawn_actor_from_class(cls,position if isinstance(position,unreal.Vector) else unreal.Vector(*position));a.set_actor_label(label);return a

    def persist(self):
        (self.out/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')

    def finish(self,status):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started;self.persist()

    def tick(self,delta):
        try:
            if time.monotonic()-self.started>180:raise RuntimeError('有限审核会话超时')
            if self.state=='warm':
                self.warm+=1
                if self.warm<35:return
                self.state='advance'
            if self.state=='advance':
                if self.steps<self.jobs[self.index]:self.component.advance_simulation(1,1/60);self.steps+=1;return
                self.state='capture1';self.since=time.monotonic();self.capture.capture_scene();return
            if time.monotonic()-self.since<.15:return
            if self.state=='capture1':
                self.capture.capture_scene();self.state='export';self.since=time.monotonic();return
            path=self.out/('Frame_'+str(self.index).zfill(3)+'.png')
            unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.out),path.name)
            if not path.exists():return
            self.report['frames'].append(dict(index=self.index,ageSeconds=self.steps/60,image=str(path),active=self.component.is_active()))
            self.persist();self.index+=1
            if self.index==len(self.jobs):self.finish('captured')
            else:self.state='advance'
        except Exception:
            self.report['error']=traceback.format_exc();self.finish('failed')


unreal._attack_slash_v2_review=AttackSlashReview(globals().get('CAPTURE_CLIP',False))
print('ATTACK_SLASH_V2_REVIEW_STARTED',str(unreal._attack_slash_v2_review.out))
