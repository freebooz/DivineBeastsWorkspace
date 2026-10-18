# -*- coding: utf-8 -*-
"""凝锋参考镜头的原生渲染审核；有限作者回调，不查询Gameplay、不承担运行播放器。

镜头来自ReferenceGuide，分辨率1536×1024。只在自有世界复用场景，切世界前保护全部脏包。
"""
from pathlib import Path
from datetime import datetime,timezone
import json,math,time,traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V3/AttackSlash'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'


class SlashV3Review:
    def __init__(self):
        self.handle=None;self.guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'))
        assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        if dirty:raise RuntimeError('UNSAVED_PACKAGES: '+str([p.get_path_name() for p in dirty]))
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        ground=self.material('M_Review_Stone',(.32,.285,.235),False)
        floor=self.spawn(unreal.StaticMeshActor,'V3_Review_Stone',(0,0,-2));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(unreal.Vector(80,80,1));floor.static_mesh_component.set_material(0,ground)
        sky=self.spawn(unreal.SkyLight,'V3_Review_Sky',(0,0,600)).get_component_by_class(unreal.SkyLightComponent);sky.set_mobility(unreal.ComponentMobility.MOVABLE);sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.set_editor_property('intensity',1.4);sky.recapture_sky()
        for label,pitch,yaw,strength in [('Key',-48,-42,5.5),('Fill',-25,80,2.0)]:
            light=self.spawn(unreal.DirectionalLight,'V3_Review_'+label,(0,0,600));light.set_actor_rotation(unreal.Rotator(pitch=pitch,yaw=yaw,roll=0),False);light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',strength)
        skyback=self.material('M_Review_Sky',(.28,.38,.45),True)
        sphere=self.spawn(unreal.StaticMeshActor,'V3_Review_SkyBackdrop',(0,0,0));sphere.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));sphere.set_actor_scale3d(unreal.Vector(1500,1500,1500));sphere.static_mesh_component.set_material(0,skyback)
        for prop in ['cast_shadow','affect_dynamic_indirect_lighting','affect_distance_field_lighting','visible_in_ray_tracing']:sphere.static_mesh_component.set_editor_property(prop,False)
        neutral=self.material('M_Review_Mannequin',(.33,.31,.28),False)
        for label,position,yaw,scale,anim in [('Caster',(195,-179,0),160,1.15,'MM_Land'),('Target',(-360,155,0),-50,1.4,'MM_Idle')]:
            a=self.spawn(unreal.SkeletalMeshActor,'V3_Review_'+label,position);a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False);a.set_actor_scale3d(unreal.Vector(scale,scale,scale))
            c=a.get_component_by_class(unreal.SkeletalMeshComponent);c.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
            for i in range(c.get_num_materials()):c.set_material(i,neutral)
            c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);c.set_animation(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/'+anim));c.set_position(.12 if label=='Caster' else .4,False)
        position=unreal.Vector(*self.guide['camera']);target=unreal.Vector(*self.guide['lookAt'])
        self.camera=self.spawn(unreal.CameraActor,'V3_Reference_Camera',position);self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,target),False)
        camera=self.camera.get_component_by_class(unreal.CameraComponent);camera.set_field_of_view(self.guide['fov']);camera.set_editor_property('aspect_ratio',1.5)
        self.pp=camera.get_editor_property('post_process_settings')
        for p,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_motion_blur_amount':True,'motion_blur_amount':0.,'override_bloom_intensity':True,'bloom_intensity':.18}.items():self.pp.set_editor_property(p,v)
        camera.set_editor_property('post_process_settings',self.pp)
        self.capactor=self.spawn(unreal.SceneCapture2D,'V3_Reference_Capture',position);self.capactor.set_actor_transform(self.camera.get_actor_transform(),False,False);self.capture=self.capactor.get_component_by_class(unreal.SceneCaptureComponent2D)
        for p,v in {'capture_every_frame':False,'capture_on_movement':False,'capture_source':unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,'post_process_settings':self.pp,'fov_angle':float(self.guide['fov'])}.items():self.capture.set_editor_property(p,v)
        # 原生游戏视图不包含编辑器未构建阴影的Preview文字；禁止将其烙入审核媒体。
        self.capture.set_editor_property('show_flag_settings',[unreal.EngineShowFlagsSetting(show_flag_name='PreviewShadowsIndicator',enabled=False)])
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1536,1024,unreal.TextureRenderTargetFormat.RTF_RGBA8);self.capture.set_editor_property('texture_target',self.target)
        self.actor=self.spawn(unreal.NiagaraActor,'V3_Review_Attack',self.guide['effectOrigin']);self.component=self.actor.get_component_by_class(unreal.NiagaraComponent);self.component.set_asset(unreal.load_asset(BASE+'/Niagara/NS_DBA_AttackSlash_V3'));self.component.set_editor_property('allow_scalability',False);self.component.set_force_solo(True)
        self.levels.pilot_level_actor(self.camera);self.levels.editor_set_viewport_realtime(True);self.levels.editor_set_game_view(True);unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)

    def material(self,name,color,unlit):
        path=BASE+'/Review/'+name;m=unreal.load_asset(path)
        if m and name!='M_Review_Stone':return m
        lib=unreal.MaterialEditingLibrary
        if m:lib.delete_all_material_expressions(m)
        else:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,BASE+'/Review',unreal.Material,unreal.MaterialFactoryNew())
        if unlit:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
        if name=='M_Review_Stone':
            pos=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition);n=lib.create_material_expression(m,unreal.MaterialExpressionCustom)
            inp=unreal.CustomInput();inp.set_editor_property('input_name','P');n.set_editor_property('inputs',[inp]);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            n.set_editor_property('code','float row=floor(P.y/90);float2 tile=frac(float2(P.x+fmod(row,2)*65,P.y)/float2(130,90));float edge=min(min(tile.x,1-tile.x),min(tile.y,1-tile.y));float joint=smoothstep(.003,.016,edge);float broad=.96+.04*sin(floor(P.x/130)*2.7+row*3.9);return float3(.32,.285,.235)*broad*lerp(.68,1,joint);');lib.connect_material_expressions(pos,'XYZ',n,'P')
        else:n=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*color,1))
        lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if unlit else unreal.MaterialProperty.MP_BASE_COLOR)
        if not unlit:
            r=lib.create_material_expression(m,unreal.MaterialExpressionConstant);r.set_editor_property('r',.72);lib.connect_material_property(r,'',unreal.MaterialProperty.MP_ROUGHNESS)
        lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False);return m

    def spawn(self,cls,label,position):
        a=self.actors.spawn_actor_from_class(cls,position if isinstance(position,unreal.Vector) else unreal.Vector(*position));a.set_actor_label(label);return a

    def start(self,clip=False,sample_steps=None):
        if self.handle:raise RuntimeError('已有有限采样回调，拒绝重入')
        assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()==self.world
        self.out=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3/RenderRuns'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');self.out.mkdir(parents=True)
        self.report=dict(status='running',manualAccepted=False,renderer='UE5.8 native SceneCapture FinalColorLDR',resolution=[1536,1024],camera=self.guide['camera'],lookAt=self.guide['lookAt'],fov=self.guide['fov'],reference='Reference_凝锋.png',performanceMeasured=False,frames=[])
        self.component.reinitialize_system();self.component.set_force_solo(True);self.component.set_component_tick_enabled(False)
        self.jobs=list(sample_steps) if sample_steps is not None else list(range(2,67,2)) if clip else [9,23,37,58]
        assert self.jobs and self.jobs==sorted(set(self.jobs)) and all(0<step<=120 for step in self.jobs)
        self.steps=0;self.index=0;self.warm=0;self.state='warm';self.started=time.monotonic();self.since=self.started
        self.capture.set_editor_property('texture_target',self.target)
        self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist();print('V3_NATIVE_REVIEW_STARTED',str(self.out))

    def persist(self):(self.out/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
    def finish(self,status):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started;self.persist()
    def tick(self,delta):
        try:
            if time.monotonic()-self.started>180:raise RuntimeError('有限渲染超时')
            if self.state=='warm':
                self.warm+=1
                if self.warm<35:return
                self.state='advance'
            if self.state=='advance':
                if self.steps<self.jobs[self.index]:self.component.advance_simulation(1,1/60);self.steps+=1;return
                self.capture.capture_scene();self.state='capture2';self.since=time.monotonic();return
            if time.monotonic()-self.since<.15:return
            if self.state=='capture2':self.capture.capture_scene();self.state='export';self.since=time.monotonic();return
            path=self.out/('Frame_'+str(self.index).zfill(3)+'.png');unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.out),path.name)
            if not path.exists():return
            self.report['frames'].append(dict(index=self.index,ageSeconds=self.steps/60,active=self.component.is_active(),image=str(path)));self.persist();self.index+=1
            if self.index==len(self.jobs):self.finish('captured')
            else:self.state='advance'
        except Exception:self.report['error']=traceback.format_exc();self.finish('failed')


if not globals().get('DEFINE_ONLY',False):
    existing=getattr(unreal,'_attack_slash_v3_review',None)
    if existing and unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()==existing.world:review=existing;review.__class__=SlashV3Review
    else:review=SlashV3Review();unreal._attack_slash_v3_review=review
    review.start(globals().get('CAPTURE_CLIP',False),globals().get('SAMPLE_STEPS'))
