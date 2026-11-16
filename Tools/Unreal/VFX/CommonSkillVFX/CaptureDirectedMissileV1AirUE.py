# -*- coding: utf-8 -*-
"""同一已校准原生镜头的有限空气开关对照；临时MID恢复后才保存自有网格。"""
from pathlib import Path
import json,time,traceback
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V1/DirectedMissile'
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1/AirCaptureRuns.json'
class AirReview:
    def __init__(self):
        assert '-DirectedMissileV1OwnedReview' in unreal.SystemLibrary.get_command_line()
        prior=getattr(unreal,'_missile_air_review',None);assert not prior or not prior.handle
        self.review=unreal._directed_missile_v1_review;assert not self.review.handle
        self.mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_DirectedMissile_V1');self.original=self.mesh.get_material(2)
        self.handle=None;self.started=time.monotonic();self.index=0
        self.report=dict(status='running',runs=[],strengths=[0.,0.,.35])
        self.mid=unreal.MaterialLibrary.create_dynamic_material_instance(self.review.world,self.original)
        self.mesh.set_material(2,self.mid)
        try:
            self.start_next();self.handle=unreal.register_slate_post_tick_callback(self.tick)
        except Exception:self.finish('failed');raise
    def persist(self):OUT.write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf8')
    def start_next(self):
        self.mid.set_scalar_parameter_value('AirWaveStrength',self.report['strengths'][self.index])
        self.review.start(sample_steps=[44]);self.persist()
    def finish(self,status):
        if self.handle:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        self.mesh.set_material(2,self.original)
        assert unreal.EditorAssetLibrary.save_loaded_asset(self.mesh,False)
        self.report.update(status=status,originalMaterialRestored=True);self.persist()
    def tick(self,dt):
        try:
            if time.monotonic()-self.started>180:raise RuntimeError('空气开关采样超时')
            if self.review.handle:return
            assert self.review.report['status']=='captured',self.review.report
            self.report['runs'].append(dict(strength=self.report['strengths'][self.index],path=str(self.review.out)))
            self.index+=1
            if self.index==3:self.finish('captured')
            else:self.start_next()
        except Exception:
            if self.review.handle:self.review.finish('failed')
            self.report['error']=traceback.format_exc();self.finish('failed')
unreal._missile_air_review=AirReview();print('MISSILE_NATIVE_AIR_AB_STARTED')
