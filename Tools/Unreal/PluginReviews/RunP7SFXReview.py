#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实SFX审核图PIE：只操作自有进程/地图的三个审核站，不保存场景，不代签人工试听。
使用-ExecCmds=py和-P7OwnedSFXReview启动；不加-NoSound，观察真实AudioComponent播放状态。
验证2D/空间/附着请求、非法参数保持既有请求、显式停止、销毁附着宿主及PIE世界退出。
"""
from pathlib import Path
import json
import time
import traceback
import unreal  # type: ignore

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012/SFXRuntime'
MAP='/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview'
PREFIX='/DBASFXPack_Core/Weather/Audio/'

class SFXReview:
    def __init__(self):
        if '-P7OwnedSFXReview' not in unreal.SystemLibrary.get_command_line():raise RuntimeError('不是受控SFX审核进程')
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        world=self.editor.get_editor_world()
        if not world or world.get_path_name().split('.')[0]!=MAP:raise RuntimeError('当前编辑器不是SFX审核图')
        self.started=time.monotonic();self.phase_start=self.started;self.phase='boot'
        self.world=None;self.stations={};self.handle=None
        self.report={'status':'running','errors':[],'checks':[],'manualAcceptance':'pending',
                     'scope':'真实音频组件与生命周期；不替代人耳试听、频响或设备性能签审'}
        OUT.mkdir(parents=True,exist_ok=True)
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
        print('P7_SFX_RUNTIME_STARTED')
    def persist(self):
        (OUT/'SFXRuntimeReview.json').write_text(json.dumps(self.report,ensure_ascii=False,indent=2),encoding='utf-8')
    def advance(self,phase):self.phase=phase;self.phase_start=time.monotonic()
    def active_audio(self):
        found=[]
        if self.world is None:return found
        world_name=self.world.get_name()
        for component in unreal.ObjectIterator(unreal.AudioComponent):
            if world_name not in component.get_path_name():continue
            sound=component.get_editor_property('sound')
            if sound and sound.get_path_name().startswith(PREFIX) and component.is_playing():found.append(component)
        return found
    def check(self,name,values):
        self.report['checks'].append({'name':name,'passed':True,**values});self.persist()
    def tick(self,delta):
        try:
            now=time.monotonic();elapsed=now-self.phase_start
            if now-self.started>150:raise RuntimeError('SFX审核超过150秒')
            if self.phase=='boot' and elapsed>2:
                self.level.editor_request_begin_play();self.advance('world')
            elif self.phase=='world':
                self.world=self.editor.get_game_world()
                if not self.world:
                    if elapsed>25:raise RuntimeError('没有PIE世界')
                    return
                actors=unreal.GameplayStatics.get_all_actors_of_class(self.world,unreal.GamePlatformSFXReviewActor)
                if len(actors)!=3:raise RuntimeError('不是预期3个音效站')
                self.stations={a.get_actor_label().removeprefix('SFXReview_'):a for a in actors}
                if set(self.stations)!={'TwoD','World','Attached'}:raise RuntimeError('场景站身份不匹配')
                self.advance('settle')
            elif self.phase=='settle' and elapsed>1:
                for name,actor in self.stations.items():
                    if not actor.play_review():raise RuntimeError('SFX服务拒绝合法定义 '+name+' '+str(actor.get_editor_property('last_result')))
                self.advance('playing')
            elif self.phase=='playing' and elapsed>1:
                active=self.active_audio()
                states={name:str(a.get_review_playback_state()) for name,a in self.stations.items()}
                if len(active)!=3 or not all(a.is_review_active() for a in self.stations.values()):
                    if elapsed<15:return
                    raise RuntimeError('没有观察到3个真实音频组件：'+str(len(active))+' '+str(states))
                if not all(a.get_review_playback_state()==unreal.GamePlatformSFXPlaybackState.PLAYING for a in self.stations.values()):
                    raise RuntimeError('组件存在但服务没有确认Playing：'+str(states))
                self.check('ThreeRealAudioComponents',{'states':states,'components':[a.get_path_name() for a in active]})
                actor=self.stations['TwoD'];before=actor.get_active_request_id().to_string()
                old=actor.get_editor_property('definition_id')
                actor.set_editor_property('definition_id',unreal.Name('/Game/InvalidLogicalIdentity'))
                # Guid的Python repr含临时结构地址，不可作为身份；使用原生Conv_GuidToString脚本方法。
                if actor.play_review() or actor.get_active_request_id().to_string()!=before or not actor.is_review_active():
                    raise RuntimeError('非法定义污染已经合法播放的声音')
                actor.set_editor_property('definition_id',old)
                self.check('InvalidDefinitionPreservesExistingRequest',{'stableRequestId':before})
                for actor in self.stations.values():actor.stop_review()
                self.advance('stopped')
            elif self.phase=='stopped' and elapsed>0.7:
                if self.active_audio() or any(a.is_review_active() for a in self.stations.values()):
                    raise RuntimeError('停止后仍有自有音频播放')
                diag=self.stations['TwoD'].get_review_diagnostics()
                if diag.active_instances!=0 or diag.pending_loads!=0:raise RuntimeError('停止后服务计数不为0')
                self.check('StopAllOwnRequests',{'active':diag.active_instances,'pending':diag.pending_loads})
                if not self.stations['Attached'].play_review():raise RuntimeError('附着声音不能再次播放')
                self.advance('destroy_owner')
            elif self.phase=='destroy_owner' and elapsed>1:
                if not self.stations['Attached'].is_review_active():raise RuntimeError('宿主销毁前附着声音未启动')
                actor=self.stations.pop('Attached');actor.destroy_actor()
                self.advance('verify_destroy')
            elif self.phase=='verify_destroy' and elapsed>0.8:
                diag=self.stations['TwoD'].get_review_diagnostics()
                if self.active_audio() or diag.active_instances!=0:raise RuntimeError('宿主销毁后音频未清理')
                self.check('OwnerEndPlayCleanup',{'active':diag.active_instances})
                self.stations.clear();self.world=None
                self.level.editor_request_end_play();self.advance('end')
            elif self.phase=='end' and elapsed>1:
                if self.editor.get_game_world() is not None:
                    if elapsed>20:raise RuntimeError('PIE世界未退出')
                    return
                self.check('WorldCleanup',{})
                self.finish('passed')
        except Exception:
            self.report['errors'].append(traceback.format_exc());self.finish('failed')
    def finish(self,status):
        if self.handle is not None:unreal.unregister_slate_post_tick_callback(self.handle);self.handle=None
        for actor in self.stations.values():
            try:actor.stop_review()
            except Exception:pass
        self.stations.clear();self.world=None
        self.report['status']=status;self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist();print('P7_SFX_RUNTIME_FINISHED',status)
        for error in self.report['errors']:unreal.log_error(error)
        if self.editor.get_game_world() is not None:self.level.editor_request_end_play()
        unreal.SystemLibrary.quit_editor()

SESSION=SFXReview()
