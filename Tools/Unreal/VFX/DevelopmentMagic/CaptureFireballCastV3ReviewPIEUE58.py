# -*- coding: utf-8 -*-
"""SSLM火球V3 P1真实UE5.8 PIE审核帧采集。仅利用当前未保存的瞬态审核地图及既有Actor，不修改正式关卡。
从游戏世界推进Niagara 22帧采样，导出4张关键帧与全时序帧；无权威玩法和客户端正式绑定。
"""
import unreal
import json
import time
import shutil
import traceback
from pathlib import Path
from datetime import datetime,timezone

ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
ASSET='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review'
OUT=ROOT/'Saved/Validation/FireballV3P1'/('PIE_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
KEYS={7:'01_HandsIgnite',15:'02_PalmsCharge',29:'03_StreamConverge',41:'04_CoreFormed'}

class NativeP1PIE:
 def __init__(self):
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  assert len(worlds)==1,'需要独占一个真实PIE审核世界'
  self.world=worlds[0]
  assert 'Untitled' in self.world.get_name() and '/Temp/UEDPIE_' in self.world.get_path_name(),'只能在瞬态审核地图运行'
  def one(cls,label):
   found=[x for x in unreal.GameplayStatics.get_all_actors_of_class(self.world,cls) if x.get_actor_label()==label]
   assert len(found)==1,'缺少独占审核对象：'+label
   return found[0]
  self.mesh=one(unreal.SkeletalMeshActor,'SSLM_P1_Casting_Mannequin').get_component_by_class(unreal.SkeletalMeshComponent)
  self.niagara=one(unreal.NiagaraActor,'SSLM_P1_CastV3').get_component_by_class(unreal.NiagaraComponent)
  self.capture=one(unreal.SceneCapture2D,'SSLM_P1_NativeCapture').get_component_by_class(unreal.SceneCaptureComponent2D)
  asset=unreal.load_asset(ASSET)
  assert isinstance(asset,unreal.NiagaraSystem)
  self.niagara.set_asset(asset)
  self.niagara.set_editor_property('allow_scalability',False)
  self.niagara.set_force_solo(True)
  self.niagara.reinitialize_system()
  self.niagara.activate(True)
  assert self.niagara.is_active(),'GPU脚本运行不成功，禁止生成假截图'
  self.niagara.set_component_tick_enabled(False)
  self.target=unreal.RenderingLibrary.create_render_target2d(self.world,1280,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
  self.capture.set_editor_property('texture_target',self.target)
  self.capture.set_editor_property('capture_every_frame',False)
  self.capture.set_editor_property('capture_on_movement',False)
  OUT.mkdir(parents=True,exist_ok=False)
  self.data={'status':'running','native':'UE5.8 PIE SceneCapture2D FinalColorLDR','gameWorld':self.world.get_path_name(),'asset':ASSET,
      'manualApproved':False,'realOnlineValidated':False,'productionLinked':False,
      'frames':[],'error':None,'gpuScriptsActivated':True,'frameRate':60,'resolution':[1280,720]}
  self.samples=list(range(1,45,2))
  self.steps=0;self.index=0;self.warm=0;self.phase='warm';self.since=time.monotonic();self.started=self.since
  self.handle=unreal.register_slate_post_tick_callback(self.tick)
  self.persist()
  print('SSLM_P1_REAL_PIE_CAPTURE_STARTED',str(OUT))
 def persist(self):
  (OUT/'Result.json').write_text(json.dumps(self.data,ensure_ascii=False,indent=2),encoding='utf-8')
 def finish(self,status):
  if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
  self.data['status']=status;self.data['elapsedSeconds']=time.monotonic()-self.started;self.persist()
  print('SSLM_P1_REAL_PIE_CAPTURE_FINISHED',status,str(OUT))
 def tick(self,dt):
  try:
   if time.monotonic()-self.started>135:raise RuntimeError('PIE原生采样超时')
   if self.phase=='warm':
    self.warm+=1
    if self.warm<25:return
    self.phase='advance'
   if self.phase=='advance':
    if self.steps<self.samples[self.index]:
     self.niagara.advance_simulation(1,1/60);self.steps+=1
     self.mesh.set_position(min(0.69,self.steps/60),False)
     return
    self.capture.capture_scene();self.since=time.monotonic();self.phase='capture2';return
   if time.monotonic()-self.since<0.13:return
   if self.phase=='capture2':
    self.capture.capture_scene();self.since=time.monotonic();self.phase='export';return
   filename='Frame_'+str(self.index).zfill(3)+'.png'
   unreal.RenderingLibrary.export_render_target(self.world,self.target,str(OUT),filename)
   f=OUT/filename
   if not f.is_file() or f.stat().st_size<3500:raise RuntimeError('未生成真实渲染帧：'+filename)
   key=KEYS.get(self.steps)
   if key:shutil.copyfile(f,OUT/(key+'.png'))
   self.data['frames'].append({'number':self.index,'seconds':round(self.steps/60,4),'file':str(f),'bytes':f.stat().st_size,'key':key})
   self.persist()
   self.index+=1
   if self.index==len(self.samples):self.finish('captured')
   else:self.phase='advance'
  except Exception:
   self.data['error']=traceback.format_exc()
   self.finish('failed')
unreal._sslm_p1_pie_review=NativeP1PIE()
