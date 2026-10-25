# -*- coding:utf-8 -*-
"""SSLM 冰霜双掌V2真正UE5.8 PIE审核：复用单掌双实例、固定曝光、4关键帧。未通过则保留失败事实。"""
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=ROOT/'Tools/Unreal/VFX/DevelopmentMagic/CapturePalmIceV1PIEUE58.py'
text=src.read_text(encoding='utf8')
replacements=[
 ('NS_DBA_PalmIce_P101_GatherReview','NS_DBA_PalmIce_P101_GatherReviewV2'),
 ('NS_DBA_PalmIce_P102_ChargeReview','NS_DBA_PalmIce_P102_ChargeReviewV2'),
 ('Saved/Validation/PalmIceV1/NativePIE','Saved/Validation/PalmIceV1/NativePIEV2'),
]
for old,new in replacements:
 assert text.count(old)>=1,'审核V1采集夹具已变化：'+old
 text=text.replace(old,new)
exec(compile(text,str(src),'exec'),{'__name__':'__main__'})
