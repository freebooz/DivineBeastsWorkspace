# -*- coding: utf-8 -*-
"""SSLM 冰法双掌聚能P1-01/02真正的UE5.8独立审核镜头。
只借用既有经过实测可见的临时PIE摄像机采集逻辑；修改运行时目标为原创PalmIce Review资产。
自身独占进程 -PalmIceReviewOwned，绝不操纵其他会话的编辑器。
"""
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
source=ROOT/'Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballP10102ScreenshotProofUE58.py'
original=source.read_text(encoding='utf8')
pairs=[
 ('PREFIX="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/"',
  'PREFIX="/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1/Niagara/"'),
 ('NS_DBA_Fireball_P1_01_HandsIgnite_Review','NS_DBA_PalmIce_P101_GatherReview'),
 ('NS_DBA_Fireball_P1_02_PalmsCharge_Review','NS_DBA_PalmIce_P102_ChargeReview'),
 ('"-P1ScreenshotStage=02"','"-PalmIceStage=02"'),
 ('"-FireballP1ScreenshotOwned"','"-PalmIceReviewOwned"'),
 ('SCALE={"01":0.65,"02":0.95}[STAGE]','SCALE={"01":0.88,"02":1.18}[STAGE]'),
 ('Saved/Validation/FireballV3P1/FinalScreenshots','Saved/Validation/PalmIceV1/NativePIE'),
 ('pos-unreal.Vector(65*SCALE,0,135*SCALE)','pos+unreal.Vector(7,0,6)'),
 ('loc-unreal.Vector(65*SCALE,0,135*SCALE)','loc+unreal.Vector(7,0,6)'),
 ('pos=unreal.Vector(390,-515,263);look=unreal.Vector(0,0,117)',
  'pos=unreal.Vector(310,-430,215);look=unreal.Vector(0,0,117)'),
 ('if self.frame>=25:','if self.frame>=27:'),
 ('if self.frame not in (9,21):return','if self.frame not in (5,11,17,24):return'),
 ('label="01_双手初燃" if STAGE=="01" else "02_双掌蓄力"',
  'label="P1-01_寒气初聚" if STAGE=="01" else "P1-02_冰霜蓄力"')
]
for old,new in pairs:
    count=original.count(old)
    if count<1:raise RuntimeError("采集夹具的必要代码片段已变更，拒绝继续："+old)
    original=original.replace(old,new)
if '-PalmIceLegacyMotionProbe' in unreal.SystemLibrary.get_command_line():
    # 仅用于验证兼容原运动模块的编译和材质可见性，不是最终向心聚能定位。
    original=original.replace('pos+unreal.Vector(7,0,6)','pos-unreal.Vector(65*SCALE,0,135*SCALE)')
    original=original.replace('loc+unreal.Vector(7,0,6)','loc-unreal.Vector(65*SCALE,0,135*SCALE)')
original=original.replace('P1Proof_Flame_','PalmIceProof_')
# 该夹具只创建瞬态Actor，严格不调用EditorAssetLibrary.save_asset或正式VFX系统。
exec(compile(original,str(source),'exec'),{'__name__':'__main__'})
