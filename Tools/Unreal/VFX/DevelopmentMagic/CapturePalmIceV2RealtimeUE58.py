# -*- coding: utf-8 -*-
"""SSLM P1冰霜V2原生GPU实时渲染对照：禁用CPU式手动逐帧模拟，仅让PIE世界自然推进GPU Niagara。
不修改系统、材质或地图，只输出真实帧，失败保留证据。
"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=root/'Tools/Unreal/VFX/DevelopmentMagic/CapturePalmIceV2PIEUE58.py'
body=src.read_text(encoding='utf8')
old="exec(compile(text,str(src),'exec'),{'__name__':'__main__'})"
assert body.count(old)==1,'V2基线夹具内容不符合预期'
adjust="""# 仅调整本轮GPU审核夹具的采集运行方式，不更改审核Niagara系统
needle="original=original.replace('P1Proof_Flame_','PalmIceProof_')"
assert text.count(needle)==1
body2="""+'''"""original=original.replace('c.set_force_solo(True)','c.set_force_solo(False)')
original=original.replace('c.advance_simulation(1,1./60.)','pass')
original=original.replace('if time.monotonic()-self.last<.17:return','if time.monotonic()-self.last<.09:return')
original=original.replace('if time.monotonic()-self.last<.15:return','if time.monotonic()-self.last<.09:return')
original=original.replace('Saved/Validation/PalmIceV1/NativePIEV2','Saved/Validation/PalmIceV1/NativePIEV2Realtime')
"""'''+"""
text=text.replace(needle,needle+'\\n'+body2)
"""
body=body.replace(old,adjust+old)
exec(compile(body,str(src),'exec'),{'__name__':'__main__'})
