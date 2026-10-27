# -*- coding: utf-8 -*-
"""原生SceneCapture调试视图尝试；必须检查结果确为热图后才作为Overdraw证据。"""
import unreal as u,runpy
from pathlib import Path
ROOT=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
d=runpy.run_path(str(ROOT/'Tools/Unreal/VFX/DevelopmentMagic/CapturePalmRedesignDualReviewUE58.py'))
class DebugReview(d['DualReview']):
 def camera(self,view):super().camera('Close')
 def sample(self):
  # 调试材质视图不把SimCache捕获当作图像生成前提；实际粒子计数有独立审核。
  self.report['samples'].append({'type':self.stages[self.stage][0],'view':self.stages[self.stage][1],'visualDebugOnly':True,'isActive':[c.is_active() for c in self.fx]})
 def begin(self):
  super().begin()
  if self.stage>=len(self.stages):return
  mode=self.stages[self.stage][1]
  self.cap.set_editor_property('show_flag_settings',[u.EngineShowFlagsSetting(show_flag_name=n,enabled=b) for n,b in [('Lighting',False),('PostProcessing',False),('Tonemapper',False),('ShaderComplexity',True),('QuadOverdraw',mode=='QuadOverdraw')]])
u._palm_debug=DebugReview(False)
u._palm_debug.stages=[(t,v) for t in ['Fire','Ice'] for v in ['ShaderComplexity','QuadOverdraw']]
print(d['OUT'])
