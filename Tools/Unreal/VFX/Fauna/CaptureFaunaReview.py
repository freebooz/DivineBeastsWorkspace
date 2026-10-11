# -*- coding: utf-8 -*-
"""独占PIE宿主的原生暂停恢复检查和三个实际游戏视角截图；不保存地图或修改物种资产。"""
from pathlib import Path
from datetime import datetime,timezone
import json,time,traceback
import unreal
ROOT=Path(__file__).resolve().parents[4]
MAP='/Game/Development/Fauna/Maps/L_DBA_FaunaReview'
PREFIX='/DBAPresentationPack_Core/VFX/Environment/Fauna/Niagara/'
class FaunaVisualReview:
    def __init__(self):
        assert '-FaunaOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
        assert '-FaunaOwnedIntegrationReview' in unreal.SystemLibrary.get_command_line()
        self.editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        self.levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        assert self.editor.get_game_world() is None
        assert self.editor.get_editor_world().get_path_name().split('.')[0]==MAP
        prior=json.loads((ROOT/'Saved/Validation/Fauna/IntegrationReview.json').read_text(encoding='utf-8'))
        assert prior['status']=='passed' and len(prior['gpuCases'])==10
        self.out=ROOT/'Saved/Validation/Fauna'/('VisualReview_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S'))
        self.out.mkdir(parents=True,exist_ok=False)
        self.started=time.monotonic();self.mark=self.started;self.phase='boot'
        self.world=None;self.components=[];self.scenes=[];self.index=-1
        self.views=('Overview','Butterfly','Fish')
        self.report={'status':'running','screenshots':[],'pauseChecks':{},'imageInspection':'pending'}
        self.handle=unreal.register_slate_post_tick_callback(self.tick)
        self.persist()
    def persist(self):
        text=json.dumps(self.report,ensure_ascii=False,indent=2)
        (self.out/'VisualReview.json').write_text(text,encoding='utf-8')
        (ROOT/'Saved/Validation/Fauna/VisualReviewLatest.json').write_text(text,encoding='utf-8')
    def snapshots(self):
        result={}
        for c in self.scenes:
            value=c.query_fauna_state()
            assert value is not None,'真实群落快照缺失'
            result[str(c.get_editor_property('species_id'))]={
                'active':bool(value[3]),'count':int(value[0]),
                'positions':[(p.x,p.y,p.z) for p in value[4]]}
        return result
    def next_view(self):
        self.index+=1
        if self.index==len(self.views):
            self.levels.editor_request_end_play()
            self.phase='end';self.mark=time.monotonic();return
        view=self.views[self.index]
        matches=[a for a in unreal.GameplayStatics.get_all_actors_of_class(self.world,unreal.CameraActor)
                 if a.get_actor_label()=='FaunaReview_Camera_'+view]
        assert len(matches)==1,'缺少唯一审核摄像机：'+view
        self.pc.set_view_target_with_blend(matches[0],0.0)
        self.phase='camera';self.mark=time.monotonic()
    def tick(self,delta):
        try:
            now=time.monotonic();elapsed=now-self.mark
            if now-self.started>150:raise RuntimeError('群落暂停/画面验收超时')
            if self.phase=='boot' and elapsed>1:
                self.levels.editor_request_begin_play();self.phase='world';self.mark=now
            elif self.phase=='world':
                self.world=self.editor.get_game_world()
                if self.world is None or elapsed<2:return
                actors=unreal.GameplayStatics.get_all_actors_of_class(self.world,unreal.Actor)
                self.scenes=[c for a in actors for c in a.get_components_by_class(unreal.DivineBeastsFaunaSceneComponent)]
                self.components=[c for a in actors for c in a.get_components_by_class(unreal.NiagaraComponent)
                                 if c.get_asset() and c.get_asset().get_path_name().startswith(PREFIX) and c.is_active()]
                if len(self.scenes)!=2 or len(self.components)!=2:return
                self.before=self.snapshots()
                assert {k:v['count'] for k,v in self.before.items()}=={'ButterflyAzure':32,'FishSilver':64}
                for c in self.components:c.set_paused(True)
                self.phase='native_pause';self.mark=now
            elif self.phase=='native_pause' and elapsed>.8:
                current=self.snapshots()
                assert all(not v['active'] and v['positions']==self.before[k]['positions'] for k,v in current.items())
                self.report['pauseChecks']['nativeNiagaraPause']={'pass':True,'cpuPositionsFrozen':True,'renderingActive':False}
                for c in self.components:c.set_paused(False)
                self.phase='native_resume';self.mark=now
            elif self.phase=='native_resume' and elapsed>.8:
                current=self.snapshots()
                assert all(v['active'] for v in current.values())
                assert current['FishSilver']['positions']!=self.before['FishSilver']['positions']
                self.report['pauseChecks']['nativeNiagaraResume']={'pass':True,'fishCpuPositionsAdvance':True}
                self.before=current
                assert unreal.GameplayStatics.set_game_paused(self.world,True)
                self.phase='world_pause';self.mark=now
            elif self.phase=='world_pause' and elapsed>.8:
                current=self.snapshots()
                assert all(v['positions']==self.before[k]['positions'] for k,v in current.items())
                self.report['pauseChecks']['gameWorldPause']={'pass':True,'cpuPositionsFrozen':True}
                assert unreal.GameplayStatics.set_game_paused(self.world,False)
                self.phase='world_resume';self.mark=now
            elif self.phase=='world_resume' and elapsed>.8:
                current=self.snapshots()
                assert current['FishSilver']['positions']!=self.before['FishSilver']['positions']
                self.report['pauseChecks']['gameWorldResume']={'pass':True,'fishCpuPositionsAdvance':True}
                self.pc=unreal.GameplayStatics.get_player_controller(self.world,0)
                assert self.pc is not None
                self.persist();self.next_view()
            elif self.phase=='camera' and elapsed>2:
                self.pending=self.out/(self.views[self.index]+'.png')
                assert not self.pending.exists()
                # 通过实际PIE PlayerController向游戏视口发出截图命令，避免截到静止编辑器世界。
                self.pc.console_command('HighResShot 1600x900 filename='+self.pending.as_posix(),True)
                self.phase='screenshot';self.mark=now
            elif self.phase=='screenshot' and elapsed>1:
                if not self.pending.exists():
                    if elapsed>20:raise RuntimeError('游戏视口未写出截图：'+str(self.pending))
                    return
                assert self.pending.stat().st_size>2000
                self.report['screenshots'].append({'view':self.views[self.index],
                    'path':self.pending.relative_to(ROOT).as_posix(),'bytes':self.pending.stat().st_size,
                    'source':'actual PIE game viewport HighResShot'})
                self.persist();self.next_view()
            elif self.phase=='end' and self.editor.get_game_world() is None:
                self.report['status']='captured';self.finish()
        except Exception:
            self.report['status']='failed';self.report['error']=traceback.format_exc();self.finish()
    def finish(self):
        unreal.unregister_slate_post_tick_callback(self.handle)
        if self.editor.get_game_world():
            unreal.GameplayStatics.set_game_paused(self.editor.get_game_world(),False)
            for c in self.components:
                if c and not c.is_being_destroyed():c.set_paused(False)
            self.levels.editor_request_end_play()
        self.report['elapsedSeconds']=time.monotonic()-self.started
        self.persist()
        print('FAUNA_VISUAL_REVIEW',self.report['status'],str(self.out))
        if self.report.get('error'):unreal.log_error(self.report['error'])
unreal._fauna_visual_review=FaunaVisualReview()
