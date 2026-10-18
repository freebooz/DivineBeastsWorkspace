# -*- coding: utf-8 -*-
"""固定参考构图下的有限编辑器运动/采样工具，不参与游戏权威弹道或生产播放。"""
from pathlib import Path
from datetime import datetime,timezone
import json,time,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile'
class MissileReview:
    def __init__(self):
        assert '-DirectedMissileV1OwnedReview' in unreal.SystemLibrary.get_command_line()
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
        assert not dirty,'先保护未保存包：'+str([p.get_path_name() for p in dirty])
        self.handle=None;self.guide=json.loads((SOURCE/'ReferenceGuide.json').read_text());self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem);self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        floor=self.spawn(unreal.StaticMeshActor,'Missile_Review_Stone',(0,0,-2));floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(unreal.Vector(70,70,1));floor.static_mesh_component.set_material(0,self.material('Stone'))
        sky=self.spawn(unreal.SkyLight,'Missile_Review_Sky',(0,0,800)).get_component_by_class(unreal.SkyLightComponent);sky.set_mobility(unreal.ComponentMobility.MOVABLE);sky.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP);sky.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'));sky.set_editor_property('intensity',1.4);sky.recapture_sky()
        for label,pitch,yaw,strength in [('Key',-48,-42,5.5),('Fill',-25,80,2.)]:
            light=self.spawn(unreal.DirectionalLight,'Missile_Review_'+label,(0,0,600));light.set_actor_rotation(unreal.Rotator(pitch=pitch,yaw=yaw,roll=0),False);c=light.get_component_by_class(unreal.DirectionalLightComponent);c.set_mobility(unreal.ComponentMobility.MOVABLE);c.set_editor_property('intensity',strength)
        sphere=self.spawn(unreal.StaticMeshActor,'Missile_Review_Backdrop',(0,0,0));sphere.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Sphere'));sphere.set_actor_scale3d(unreal.Vector(1500,1500,1500));sphere.static_mesh_component.set_material(0,self.material('Sky'))
        for p in ('cast_shadow','affect_dynamic_indirect_lighting','affect_distance_field_lighting','visible_in_ray_tracing'):sphere.static_mesh_component.set_editor_property(p,False)
        neutral=self.material('Mannequin')
        for label,key,face in [('Caster','casterFoot','target'),('Target','targetFoot','source')]:
            a=self.spawn(unreal.SkeletalMeshActor,'Missile_Review_'+label,self.guide[key]);rot=unreal.MathLibrary.find_look_at_rotation(a.get_actor_location(),unreal.Vector(*self.guide[face]));a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=rot.yaw,roll=0),False)
            c=a.get_component_by_class(unreal.SkeletalMeshComponent);c.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
            for i in range(c.get_num_materials()):c.set_material(i,neutral)
            c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE);c.set_animation(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Animations/WorldLocomotion/MM_Idle'));c.set_position(.4,False)
        position=unreal.Vector(*self.guide['camera']);target=unreal.Vector(*self.guide['lookAt']);self.camera=self.spawn(unreal.CameraActor,'Missile_Reference_Camera',self.guide['camera']);self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(position,target),False)
        cam=self.camera.get_component_by_class(unreal.CameraComponent);cam.set_field_of_view(self.guide['fov']);cam.set_editor_property('aspect_ratio',1656/1002);pp=cam.get_editor_property('post_process_settings')
        for p,v in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_motion_blur_amount':True,'motion_blur_amount':0.,'override_bloom_intensity':True,'bloom_intensity':.18}.items():pp.set_editor_property(p,v)
        cam.set_editor_property('post_process_settings',pp);cap=self.spawn(unreal.SceneCapture2D,'Missile_Reference_Capture',self.guide['camera']);cap.set_actor_transform(self.camera.get_actor_transform(),False,False);self.capture=cap.get_component_by_class(unreal.SceneCaptureComponent2D)
        for p,v in {'capture_every_frame':False,'capture_on_movement':False,'capture_source':unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR,'post_process_settings':pp,'fov_angle':45.}.items():self.capture.set_editor_property(p,v)
        self.capture.set_editor_property('show_flag_settings',[unreal.EngineShowFlagsSetting(show_flag_name='PreviewShadowsIndicator',enabled=False)])
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1656,1002,unreal.TextureRenderTargetFormat.RTF_RGBA8);self.capture.set_editor_property('texture_target',self.target)
        self.actor=self.spawn(unreal.NiagaraActor,'Missile_Review_Attack',self.guide['source']);self.actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*self.guide['source']),unreal.Vector(*self.guide['target'])),False);self.component=self.actor.get_component_by_class(unreal.NiagaraComponent);self.component.set_asset(unreal.load_asset(BASE+'/Niagara/NS_DBA_DirectedMissile_V1'));self.component.set_editor_property('allow_scalability',False);self.component.set_force_solo(True)
        levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.pilot_level_actor(self.camera);levels.editor_set_viewport_realtime(True);levels.editor_set_game_view(True)
    def material(self,kind):
        path=BASE+'/Review/M_Review_'+kind;m=unreal.load_asset(path)
        if m:return m
        m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(path.split('/')[-1],BASE+'/Review',unreal.Material,unreal.MaterialFactoryNew());lib=unreal.MaterialEditingLibrary
        if kind=='Sky':m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('two_sided',True)
        if kind=='Stone':
            pos=lib.create_material_expression(m,unreal.MaterialExpressionWorldPosition);n=lib.create_material_expression(m,unreal.MaterialExpressionCustom);i=unreal.CustomInput();i.set_editor_property('input_name','P');n.set_editor_property('inputs',[i]);n.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);n.set_editor_property('code','float2 t=frac(P.xy/float2(130,100));float e=min(min(t.x,1-t.x),min(t.y,1-t.y));float ring=abs(frac(length(P.xy)/105)-.5);return float3(.36,.33,.265)*lerp(.72,1,smoothstep(.004,.017,e))*lerp(.85,1,smoothstep(.005,.018,ring));');lib.connect_material_expressions(pos,'XYZ',n,'P')
        else:n=lib.create_material_expression(m,unreal.MaterialExpressionConstant3Vector);n.set_editor_property('constant',unreal.LinearColor(*((.28,.38,.45) if kind=='Sky' else (.38,.35,.30)),1))
        lib.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR if kind=='Sky' else unreal.MaterialProperty.MP_BASE_COLOR);lib.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m,False);return m
    def spawn(self,cls,label,location):
        a=self.actors.spawn_actor_from_class(cls,unreal.Vector(*location));a.set_actor_label(label);return a
    def start(self,clip=False,sample_steps=None):
        assert not self.handle
        self.capture.set_editor_property('texture_target',self.target)
        self.out=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1/RenderRuns'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');self.out.mkdir(parents=True)
        definition=unreal.load_asset(BASE+'/Definitions/DA_DBA_VFX_DirectedMissileV1_Review')
        for name,value in definition.get_editor_property('default_parameters').get_editor_property('float_parameters').items():self.component.set_variable_float(name,value)
        self.component.set_world_location(unreal.Vector(*self.guide['source']),False,False);self.component.set_variable_vec3('User.Direction',unreal.Vector(*self.guide['direction']));self.component.reinitialize_system();self.component.set_component_tick_enabled(False)
        self.jobs=list(sample_steps) if sample_steps is not None else list(range(2,121,2)) if clip else [12,30,44,48,110]
        assert self.jobs and self.jobs==sorted(set(self.jobs)) and all(0<i<=120 for i in self.jobs)
        self.index=0;self.step=0;self.warm=0;self.state='warm';self.started=time.monotonic();self.since=self.started
        self.report=dict(status='running',resolution=[1656,1002],camera=self.guide['camera'],lookAt=self.guide['lookAt'],fov=45,source=self.guide['source'],target=self.guide['target'],flightDuration=.75,renderer='native SceneCapture FinalColorLDR',motionScope='finite editor review transform, not server projectile',manualAccepted=False,performanceMeasured=False,frames=[])
        self.handle=unreal.register_slate_post_tick_callback(self.tick);self.persist();print('MISSILE_NATIVE_CAPTURE_STARTED',str(self.out))
    def persist(self):(self.out/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
    def finish(self,status):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status;self.persist()
    def tick(self,dt):
        try:
            if time.monotonic()-self.started>210:raise RuntimeError('有限审核采样超时')
            if self.state=='warm':
                self.warm+=1
                if self.warm<25:return
                self.state='advance'
            if self.state=='advance':
                if self.step<self.jobs[self.index]:
                    for _ in range(min(4,self.jobs[self.index]-self.step)):
                        self.step+=1;t=min(1,self.step/60/.75);a=unreal.Vector(*self.guide['source']);b=unreal.Vector(*self.guide['target']);self.component.set_world_location(a+(b-a)*t,False,False);self.component.advance_simulation(1,1/60)
                    return
                self.capture.capture_scene();self.state='capture2';self.since=time.monotonic();return
            if time.monotonic()-self.since<.10:return
            if self.state=='capture2':self.capture.capture_scene();self.state='export';self.since=time.monotonic();return
            p=self.out/('Frame_'+str(self.index).zfill(3)+'.png');unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.out),p.name)
            if not p.exists():return
            self.report['frames'].append(dict(index=self.index,ageSeconds=self.step/60,active=self.component.is_active(),headPosition=list(self.component.get_world_location().to_tuple()),image=str(p)));self.persist();self.index+=1
            if self.index==len(self.jobs):self.finish('captured')
            else:self.state='advance'
        except Exception:self.report['error']=traceback.format_exc();self.finish('failed')
if not globals().get('DEFINE_ONLY',False):
    r=getattr(unreal,'_directed_missile_v1_review',None)
    if r and unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()==r.world:r.__class__=MissileReview
    else:r=MissileReview();unreal._directed_missile_v1_review=r
    r.start(globals().get('CAPTURE_CLIP',False),globals().get('SAMPLE_STEPS'))
