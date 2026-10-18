# -*- coding: utf-8 -*-
"""通用VFX原生视口审核：固定相机/曝光、真实GPU步进、有限回调、逐模板关键帧。

Monolith引擎内调用，只创建本任务审核地图；不保存或修改原地图/出生点。
回调属于短期制作工具，结束即解绑，不轮询任何业务或UI状态。
"""
from pathlib import Path
from datetime import datetime,timezone
import json
import math
import time
import traceback
import unreal

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/Sources.json'


class CommonVFXReview:
    """一次审核会话拥有独立临时世界、相机和时钟，失败保留日志并释放回调。"""
    def __init__(self,keys=None,samples=(.35,),save_map=False):
        self.handle=None
        assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
        assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
        old=getattr(unreal,'_common_vfx_review',None)
        if old and old.handle:raise RuntimeError('已有审核回调，禁止重入')
        dirty=unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()
        if dirty:raise RuntimeError('存在未保存内容包，不切换审核世界：'+','.join(x.get_name() for x in dirty))
        self.rows=[r for r in json.loads(SOURCE.read_text(encoding='utf8'))['templates'] if not keys or r['key'] in keys]
        for r in self.rows:
            if not unreal.EditorAssetLibrary.does_asset_exist(BASE+'/Niagara/NS_DBA_Common_'+r['key']):raise RuntimeError('缺少System：'+r['key'])
        self.run_id=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        self.out=ROOT/'Saved/Validation/CommonSkillVFX/RenderRuns'/self.run_id;self.out.mkdir(parents=True)
        self.report=dict(status='running',renderer='UE5.8 native SceneCapture FinalColorLDR GPU',manualAccepted=False,gameplayVerified=False,cases=[],resolution=[1280,800],samples=list(samples))
        self.actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
        floor=self.spawn(unreal.StaticMeshActor,'CommonVFX_Floor',(0,0,-2))
        floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'));floor.set_actor_scale3d(unreal.Vector(30,30,1))
        ground_path=BASE+'/Review/M_DBA_CommonVFX_ReviewGround'
        ground=unreal.load_asset(ground_path)
        if ground is None:
            ground=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_DBA_CommonVFX_ReviewGround',BASE+'/Review',unreal.Material,unreal.MaterialFactoryNew())
            lib=unreal.MaterialEditingLibrary;n=lib.create_material_expression(ground,unreal.MaterialExpressionConstant3Vector)
            n.set_editor_property('constant',unreal.LinearColor(.30,.29,.25,1));lib.connect_material_property(n,'',unreal.MaterialProperty.MP_BASE_COLOR)
            rough=lib.create_material_expression(ground,unreal.MaterialExpressionConstant);rough.set_editor_property('r',.82);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
            lib.recompile_material(ground);assert unreal.EditorAssetLibrary.save_loaded_asset(ground,False)
        floor.static_mesh_component.set_material(0,ground)
        for label,rotation,strength in [('Key',(-48,-30,0),4.5),('Fill',(-40,145,0),1.2)]:
            light=self.spawn(unreal.DirectionalLight,'CommonVFX_'+label,(0,0,600));light.set_actor_rotation(unreal.Rotator(*rotation),False)
            light.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property('intensity',strength)
        self.mannequin=self.spawn(unreal.SkeletalMeshActor,'CommonVFX_Mannequin',(0,0,0))
        sk=self.mannequin.get_component_by_class(unreal.SkeletalMeshComponent)
        sk.set_skeletal_mesh_asset(unreal.load_asset('/DBAContentPack_Common/Mannequins/DBA/Meshes/SKM_Manny_Simple'))
        self.camera=self.spawn(unreal.CameraActor,'CommonVFX_Camera',(570,-730,650))
        camera=self.camera.get_component_by_class(unreal.CameraComponent);camera.set_field_of_view(45)
        pp=camera.get_editor_property('post_process_settings')
        for prop,value in {'override_auto_exposure_method':True,'auto_exposure_method':unreal.AutoExposureMethod.AEM_MANUAL,'override_auto_exposure_bias':True,'auto_exposure_bias':0.,'override_auto_exposure_apply_physical_camera_exposure':True,'auto_exposure_apply_physical_camera_exposure':False,'override_motion_blur_amount':True,'motion_blur_amount':0.,'override_bloom_intensity':True,'bloom_intensity':.20}.items():pp.set_editor_property(prop,value)
        camera.set_editor_property('post_process_settings',pp)
        capture_actor=self.spawn(unreal.SceneCapture2D,'CommonVFX_SceneCapture',(570,-730,650))
        self.capture_actor=capture_actor;self.capture=capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
        self.capture.set_editor_property('capture_every_frame',False);self.capture.set_editor_property('capture_on_movement',False)
        self.capture.set_editor_property('capture_source',unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        self.capture.set_editor_property('post_process_settings',pp);self.capture.set_editor_property('fov_angle',45)
        self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,800,unreal.TextureRenderTargetFormat.RTF_RGBA8)
        self.capture.set_editor_property('texture_target',self.target)
        self.effect=self.spawn(unreal.NiagaraActor,'CommonVFX_Review',(0,0,0));self.component=self.effect.get_component_by_class(unreal.NiagaraComponent)
        self.component.set_editor_property('allow_scalability',False);self.component.set_force_solo(True)
        self.levels.pilot_level_actor(self.camera);self.levels.editor_set_viewport_realtime(True);self.levels.editor_set_game_view(True)
        unreal.AutomationLibrary.set_editor_active_viewport_view_mode(unreal.ViewModeIndex.VMI_LIT)
        self.jobs=[(r,f) for r in self.rows for f in samples];self.index=-1;self.frame=0;self.started=time.monotonic();self.pending=None
        self.save_map=save_map;self.next_case();self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()

    def spawn(self,cls,label,location):
        actor=self.actors.spawn_actor_from_class(cls,unreal.Vector(*location));actor.set_actor_label(label);return actor

    def next_case(self):
        self.index+=1
        if self.index>=len(self.jobs):
            self.finish('captured');return
        row,fraction=self.jobs[self.index];self.row=row;self.fraction=fraction
        center=unreal.Vector(100 if row['key'] in ('Movement_Blink','Attack_Pierce') else -50 if row['category']=='Movement' else 0,0,105)
        offset=unreal.Vector(570,-730,650)
        if row['key'] in ('Movement_Blink','Movement_Leap','Attack_Pierce','Healing_Link'):offset*=1.3
        self.camera.set_actor_location(center+offset,False,False)
        self.camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(self.camera.get_actor_location(),center),False)
        self.capture_actor.set_actor_transform(self.camera.get_actor_transform(),False,False)
        self.component.set_asset(unreal.load_asset(BASE+'/Niagara/NS_DBA_Common_'+row['key']))
        self.component.reinitialize_system();self.component.set_force_solo(True);self.component.set_component_tick_enabled(False)
        self.target_steps=max(8,round(row['duration']*fraction*60));self.steps=0;self.warm_frames=0;self.primed=False;self.pending=None;self.since=time.monotonic()
        self.path=self.out/(row['key']+'_'+str(int(fraction*100)).zfill(2)+'.png')

    def persist(self):
        (self.out/'Result.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')

    def finish(self,status):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist()

    def tick(self,delta):
        try:
            self.frame+=1
            if time.monotonic()-self.since>75:raise RuntimeError('单帧审核超时：'+self.row['key'])
            if self.steps<self.target_steps:
                self.component.advance_simulation(1,1/60);self.steps+=1;return
            self.warm_frames+=1
            if self.warm_frames<8:return
            if self.pending is None:
                self.capture.capture_scene();self.pending=True
                self.capture_time=time.monotonic();return
            if time.monotonic()-self.capture_time<.75:return
            if not self.primed:self.primed=True;self.pending=None;return
            unreal.RenderingLibrary.export_render_target(self.world,self.target,str(self.out),self.path.name)
            if not self.path.exists():return
            self.report['cases'].append(dict(key=self.row['key'],ageSeconds=self.target_steps/60,active=self.component.is_active(),frame=self.frame,image=str(self.path),manualAccepted=False))
            self.persist();self.next_case()
        except Exception:
            self.report['error']=traceback.format_exc();self.finish('failed')


unreal._common_vfx_review=CommonVFXReview(globals().get('CAPTURE_KEYS'),globals().get('SAMPLES',(.35,)),globals().get('SAVE_MAP',False))
print('COMMON_VFX_REVIEW_STARTED',str(unreal._common_vfx_review.out))
