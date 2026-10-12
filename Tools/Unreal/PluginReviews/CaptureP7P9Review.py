#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三批已有审核图的真实RHI截图：单进程单图，不改地图/光照/UI、不代签人工。
仅允许-P7OwnedVisualCapture且预期地图已经打开；Slate异步等待GPU与HighResShot写盘。
"""
from pathlib import Path
import json
import os
import struct
import time
import traceback
import unreal  # type: ignore
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012/Visual'
TASK=os.environ.get('P7P9_CAPTURE_TASK','heroes')
MAPS={'heroes':'/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase',
      'sfx':'/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview'}
class Capture:
    def __init__(self):
        if TASK not in MAPS or '-P7OwnedVisualCapture' not in unreal.SystemLibrary.get_command_line():
            raise RuntimeError('只允许独立P7图形取证进程')
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world=self.editor.get_editor_world()
        if not world or world.get_path_name().split('.')[0]!=MAPS[TASK]:raise RuntimeError('不是约定审核关卡')
        cameras=[x for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                 if isinstance(x,unreal.CameraActor) and x.get_actor_label().endswith('_Camera')]
        if len(cameras)!=1:raise RuntimeError('无法确定唯一审核摄像机')
        self.camera=cameras[0]
        OUT.mkdir(parents=True,exist_ok=True)
        self.path=OUT/(TASK+'_UE_RHI.png')
        if self.path.exists():raise RuntimeError('截图目标已存在，避免旧图冒充新结果')
        self.started=time.monotonic();self.mark=self.started;self.phase='warmup';self.job=None
        self.report={'status':'running','map':MAPS[TASK],'manualAcceptance':'pending','source':'UE5.8真实编辑器RHI摄像机'}
        self.level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        self.level.editor_set_viewport_realtime(True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
    def tick(self,delta):
        try:
            now=time.monotonic()
            if now-self.started>120:raise RuntimeError('真实图形截图超时')
            if self.phase=='warmup' and now-self.started>7:
                self.job=unreal.AutomationLibrary.take_high_res_screenshot(1920,1080,str(self.path),
                    camera=self.camera,mask_enabled=False,capture_hdr=False,delay=.2,force_game_view=True)
                if self.job is None:raise RuntimeError('引擎未受理截图')
                self.phase='capture';self.mark=now
            elif self.phase=='capture' and now-self.mark>2 and self.path.exists():
                data=self.path.read_bytes()
                if len(data)<10000 or data[:8]!=b'\x89PNG\r\n\x1a\n':raise RuntimeError('没有生成有效PNG')
                w,h=struct.unpack('>II',data[16:24])
                if (w,h)!=(1920,1080):raise RuntimeError('截图分辨率不符')
                self.report.update(width=w,height=h,bytes=len(data),image=self.path.relative_to(ROOT).as_posix())
                self.finish('captured')
        except Exception:
            self.report['error']=traceback.format_exc();self.finish('failed')
    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.report['status']=status;self.report['seconds']=time.monotonic()-self.started
        (OUT/(TASK+'_Capture.json')).write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
        print('P7P9_RHI_CAPTURE_FINISHED',TASK,status)
        unreal.SystemLibrary.quit_editor()
SESSION=Capture()
