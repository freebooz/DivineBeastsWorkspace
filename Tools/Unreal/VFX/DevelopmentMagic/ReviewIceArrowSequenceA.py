#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 寒冰箭候选全过程原生渲染录像夹具，由 Monolith 调用。

只在专属编辑器临时世界驱动三个候选 Niagara，按24fps推进并逐帧原生截图。
0—0.9秒凝聚，0.9—1.5秒视觉飞行，1.5秒命中，随后观察至2.8秒排空。
这是编辑器视觉时序验证，不是角色动画、联网命中、正式客户端或性能验收。
命中只写视觉停止参数并停止轨迹发射，保留历史轨迹自然消散；不访问服务器判定。
"""
from pathlib import Path
from datetime import datetime, timezone
from uuid import uuid4
import importlib.util
import json
import time
import traceback
import unreal

ROOT = Path(__file__).resolve().parents[4]
BASE = '/DBAPresentationPack_Core/VFX/IceArrow/Niagara/Review/'
RUNS_ROOT = ROOT / 'Saved/Validation/IceArrow/Production_20261016/SequenceRuns'


def reserve_sequence_directory(root=RUNS_ROOT):
    """每次审查独占一个新目录，严禁覆盖旧审核帧、失败报告和人工签收证据。"""
    run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ') + '_' + uuid4().hex[:8]
    output = root / run_id
    output.mkdir(parents=True, exist_ok=False)
    return run_id, output


class SequenceReview:
    """拥有一次固定步长渲染回调；异常时注销并保留失败证据，不自动覆盖旧帧。"""

    def __init__(self):
        assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
        old_review = getattr(unreal, '_ice_sequence_review', None)
        if old_review is not None and old_review.handle:
            raise RuntimeError('已有连续审核正在运行，本次不得并行开启')
        self.run_id, self.output_dir = reserve_sequence_directory()
        spec = importlib.util.spec_from_file_location('ice_sequence_view', Path(__file__).with_name('ReviewIceArrowViewport.py'))
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        self.fixture = module.IceArrowViewportReview('Cast', 'SequenceA_Setup_' + self.run_id, .0, BASE+'NS_DBA_IceArrow_Cast_A', False, True)
        self.fixture.finish('交由连续渲染夹具接管')
        self.cast = self.fixture.component
        self.cast.deactivate()
        self.cast.set_visibility(False, True)
        self.fixture.effect_actor.set_actor_location(unreal.Vector(-220,0,0),False,False)
        self.flight_actor, self.flight = self.spawn('Flight', unreal.Vector(-182,0,104))
        self.impact_actor, self.impact = self.spawn('Impact', unreal.Vector(170,0,104))
        camera = self.fixture.camera
        camera.set_actor_location(unreal.Vector(-340,900,420),False,False)
        camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(-30,0,105)),False)
        camera.get_component_by_class(unreal.CameraComponent).set_field_of_view(46)
        self.frame = -1
        self.started = time.monotonic()
        self.pending = None
        self.requested = 0
        self.handle = unreal.register_slate_post_tick_callback(self.tick)
        self.events = []
        self.flight_started = False
        self.hit = False
        self.drained = False
        self.primed = False
        self.write('运行中')

    def spawn(self, stage, location):
        """生成纯审核组件；关闭自动时钟，生命周期由本次夹具唯一管理。"""
        actor = self.fixture.spawn(unreal.NiagaraActor,'IceSequence_'+stage,location)
        component = actor.get_component_by_class(unreal.NiagaraComponent)
        component.set_asset(unreal.load_asset(BASE+'NS_DBA_IceArrow_'+stage+'_A'))
        component.set_editor_property('allow_scalability',False)
        component.set_force_solo(True)
        component.deactivate()
        component.set_visibility(False, True)
        component.set_component_tick_enabled(False)
        return actor, component

    def write(self, status, error=None):
        """保存真实帧数、事件时钟与验收边界；不把视频生成当成视觉通过。"""
        report={'状态':status,'运行标识':self.run_id,'输出目录':str(self.output_dir),
                '帧率':24,'时长秒':2.8,'实际帧数':len(list(self.output_dir.glob('Frame_*.png'))),
                '事件':self.events,'来源':'UE5.8原生视口截图；编辑器视觉夹具','游戏运行验证':False,'人工视觉验收':False}
        if error: report['错误']=error
        (self.output_dir/'SequenceReport.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')

    def stop(self, status, error=None):
        """结束回调，保留审核世界；不循环播放、不残留后台录制。"""
        if self.handle:
            unreal.unregister_slate_post_tick_callback(self.handle); self.handle=None
        self.write(status,error)

    def screenshot(self, path):
        """原生截图触发真实GPU绘制；输出文件存在后才允许推进下一帧。"""
        self.pending=path
        self.requested=time.monotonic()
        task=unreal.AutomationLibrary.take_high_res_screenshot(1280,720,str(path),camera=self.fixture.camera,
                mask_enabled=False,capture_hdr=False,delay=.02,force_game_view=True)
        if task is None: raise RuntimeError('原生截图任务未建立')

    def tick(self, delta_seconds):
        """状态机按模拟时钟推进，壁钟只用于截图完成检测与超时。"""
        try:
            elapsed=time.monotonic()-self.started
            if elapsed>180: raise RuntimeError('连续渲染超时')
            if elapsed<3: return
            if self.pending:
                capture_elapsed = time.monotonic() - self.requested
                if capture_elapsed > 30:
                    raise RuntimeError('截图未在30秒内保存：' + str(self.pending))
                if not self.pending.exists() or self.pending.stat().st_size == 0 or capture_elapsed < .15: return
                self.pending=None
            if not self.primed:
                self.primed=True
                self.screenshot(self.output_dir/'Warmup.png')
                return
            self.frame+=1
            t=self.frame/24.0
            if t>2.8:
                self.stop('已输出连续原生帧，待视觉审核'); return
            if self.frame==0:
                self.cast.set_visibility(True, True)
                self.cast.reinitialize_system(); self.cast.set_component_tick_enabled(False)
                self.events.append({'秒':t,'事件':'开始凝聚'})
            if t>=.9 and not self.flight_started:
                self.cast.deactivate(); self.cast.set_visibility(False, True)
                self.flight.set_visibility(True, True)
                self.flight.reinitialize_system(); self.flight.set_component_tick_enabled(False)
                self.flight_started=True
                self.events.append({'秒':t,'事件':'释放与飞行'})
            if t>=1.5 and not self.hit:
                self.flight.set_variable_float('User.StopFlight',1.0)
                self.flight.set_emitter_enable('HistoryTrail',False)
                self.impact.set_visibility(True, True)
                self.impact.reinitialize_system(); self.impact.set_component_tick_enabled(False)
                self.hit=True
                self.events.append({'秒':t,'事件':'命中；停止网格和尾迹新粒子'})
            if t>=2.0 and not self.drained:
                self.flight.deactivate(); self.flight.set_visibility(False, True)
                self.drained=True
                self.events.append({'秒':t,'事件':'轨迹排空窗口结束'})
            if not self.flight_started: self.cast.advance_simulation(1,1/24.0)
            if self.flight_started and not self.drained:
                if not self.hit:
                    x=-182+352*min(1,(t-.9)/.6)
                    self.flight_actor.set_actor_location(unreal.Vector(x,0,104),False,False)
                self.flight.advance_simulation(1,1/24.0)
            if self.hit: self.impact.advance_simulation(1,1/24.0)
            self.screenshot(self.output_dir/f'Frame_{self.frame:04d}.png')
        except Exception:
            self.stop('失败',traceback.format_exc())


if __name__=='__main__':
    unreal._ice_sequence_review=SequenceReview()
