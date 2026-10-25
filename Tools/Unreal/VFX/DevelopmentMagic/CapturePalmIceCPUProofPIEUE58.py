# -*- coding: utf-8 -*-
"""冰霜P1 CPU聚能诊断原型的真实UE5.8双掌截图，严禁将诊断图冒充正式视觉验收。"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=root/'Tools/Unreal/VFX/DevelopmentMagic/CapturePalmIceV2PIEUE58.py'
body=src.read_text(encoding='utf8')
old='NS_DBA_PalmIce_P101_GatherReviewV2'
assert body.count(old)==1
body=body.replace(old,'NS_DBA_PalmIce_P101_CPUProof')
old='Saved/Validation/PalmIceV1/NativePIEV2'
assert body.count(old)==1
body=body.replace(old,'Saved/Validation/PalmIceV1/NativePIECPUProof')
exec(compile(body,str(src),'exec'),{'__name__':'__main__'})
