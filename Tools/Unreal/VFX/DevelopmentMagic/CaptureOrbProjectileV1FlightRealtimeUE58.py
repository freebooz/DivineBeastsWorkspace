# -*- coding: utf-8 -*-
"""SSLM 火冰弹体V1: 自然实时PIE更新对照，禁用手动GPU推进。
只验证审查地图里按预设轨迹运动的NiagaraActor和Ribbon历史尾迹；不是权威弹道接线证据。
"""
from pathlib import Path
import unreal,re
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=root/'Tools/Unreal/VFX/DevelopmentMagic/CaptureOrbProjectileV1FlightPIEUE58.py'
wrapper=src.read_text(encoding='utf8')
old="exec(compile(source,str(src),'exec'),{'__name__':'__main__'})"
assert wrapper.count(old)==1,'审核视觉夹具版本变化'
adjust="""
# GPU让真实PIE在每帧自然Update，并保持组件正常Tick。
import re
source=source.replace('self.comp.set_component_tick_enabled(False)','self.comp.set_component_tick_enabled(True)')
source=source.replace('self.comp.advance_simulation(1,1/60)','pass')
source=source.replace('self.max_frames=68','self.max_frames=103')
source=source.replace('self.frames*4.1','self.frames*2.6')
source=source.replace('Saved/Validation/OrbProjectileV1/NativeFlightPIE','Saved/Validation/OrbProjectileV1/NativeFlightPIERealtime')
stages={26:'F2_First',54:'F2_Trail',78:'F2_Mid',100:'F2_Final'} if not ice else {26:'I2_First',54:'I2_Trail',78:'I2_Mid',100:'I2_Final'}
source=re.sub(r'^STAGES=.*$', 'STAGES='+repr(stages),source,count=1,flags=re.MULTILINE)
"""
wrapper=wrapper.replace(old,adjust+'\n'+old)
exec(compile(wrapper,str(src),'exec'),{'__name__':'__main__'})
