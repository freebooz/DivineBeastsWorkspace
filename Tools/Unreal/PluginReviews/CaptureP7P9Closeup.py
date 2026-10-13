#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P7/P8 审核单体近景真实UE RHI证据：只读拍摄，不保存、编译或改变正式游戏地图。

启动参数 -P7OwnedCloseupCapture，环境变量P7P9_CLOSEUP_TASK=heroes/sfx、
P7P9_CLOSEUP_ID=Rat/World等。按独立审核地图和真实CameraActor标签精确绑定。
照片是只读画面证据，不等于人工签收；原图自动保存并校验PNG尺寸。
"""
from __future__ import annotations
import json
import os
import struct
import time
import traceback
from pathlib import Path

import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/"Saved/Validation/PluginP7P9_20261012/Visual/Closeups"
TASK=os.environ.get("P7P9_CLOSEUP_TASK","")
IDENT=os.environ.get("P7P9_CLOSEUP_ID","")
ROLES={
    "heroes":("L_DBA_HeroShowcase","ReviewHeroCloseup_",
              {"Rat","Ox","Tiger","Rabbit","Dragon","Snake","Horse","Goat","Monkey","Rooster","Dog","Boar"}),
    "sfx":("L_GP_SFXReview","ReviewSFXCloseup_",{"TwoD","World","Attached"}),
}


class EvidenceCapture:
    def __init__(self):
        if "-P7OwnedCloseupCapture" not in unreal.SystemLibrary.get_command_line():
            raise RuntimeError("缺少显式近景图形审核进程授权")
        if TASK not in ROLES or IDENT not in ROLES[TASK][2]:
            raise RuntimeError("非法近景任务或对象身份")
        if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
            raise RuntimeError("引擎版本错误")
        project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
        if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
            raise RuntimeError("不是锁定工作空间")
        map_name,prefix,_=ROLES[TASK]
        expected_map="/Game/Development/Foundation/PluginReviews/Maps/"+map_name
        editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world=editor.get_editor_world()
        if not world or world.get_path_name().split(".")[0]!=expected_map:
            raise RuntimeError("当前World不属于指定审核地图："+expected_map)
        candidates=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                    if isinstance(a,unreal.CameraActor) and a.get_actor_label()==prefix+IDENT]
        if len(candidates)!=1:
            raise RuntimeError("没有唯一真实近景CameraActor："+prefix+IDENT)
        self.camera=candidates[0]
        OUT.mkdir(parents=True,exist_ok=True)
        self.path=OUT/(TASK+"_"+IDENT+".png")
        self.report_path=OUT/(TASK+"_"+IDENT+".json")
        if self.path.exists() or self.report_path.exists():
            raise RuntimeError("已存在近景截图，拒绝覆盖旧证据")
        self.started=time.monotonic()
        self.phase="warmup"
        self.captured_at=self.started
        self.report={
            "status":"running","map":expected_map,"camera":self.camera.get_actor_label(),
            "manualAcceptance":"pending","scope":"真实UE5.8 RHI近景拍摄；不代表音质/3A美术/人工签审",
        }
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)

    def finish(self,status):
        unreal.unregister_slate_post_tick_callback(self.handle)
        self.report["status"]=status
        self.report["elapsedSeconds"]=round(time.monotonic()-self.started,2)
        self.report_path.write_text(json.dumps(self.report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print("P7P9_CLOSEUP_CAPTURE_FINISHED",TASK,IDENT,status)
        unreal.SystemLibrary.quit_editor()

    def tick(self,delta):
        try:
            elapsed=time.monotonic()-self.started
            if elapsed>95:raise RuntimeError("近景真实RHI截图超时")
            if self.phase=="warmup" and elapsed>=7:
                self.job=unreal.AutomationLibrary.take_high_res_screenshot(
                    1920,1080,str(self.path),camera=self.camera,
                    mask_enabled=False,capture_hdr=False,delay=.2,force_game_view=True)
                if self.job is None:raise RuntimeError("UE没有受理截图")
                self.phase="capture";self.captured_at=time.monotonic()
            elif self.phase=="capture" and time.monotonic()-self.captured_at>2 and self.path.exists():
                raw=self.path.read_bytes()
                if len(raw)<10000 or raw[:8]!=b"\x89PNG\r\n\x1a\n":
                    raise RuntimeError("引擎未生成有效PNG截图")
                width,height=struct.unpack(">II",raw[16:24])
                if (width,height)!=(1920,1080):
                    raise RuntimeError("截图尺寸不符")
                self.report.update(image=self.path.relative_to(ROOT).as_posix(),
                                   width=width,height=height,bytes=len(raw))
                self.finish("captured")
        except Exception:
            self.report["error"]=traceback.format_exc()
            self.finish("failed")


CAPTURE=EvidenceCapture()
