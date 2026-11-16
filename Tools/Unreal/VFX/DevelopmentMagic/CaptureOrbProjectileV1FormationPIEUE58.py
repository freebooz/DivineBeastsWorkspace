# -*- coding: utf-8 -*-
"""SSLM 火球/冰块成形 V1 独立UE5.8原生PIE截图夹具。
基于现有获证据的SceneCapture审核脚本，只改变目标审核资产和关键帧；
不改变正式地图/GAS/权威弹体，也不使用AI概念图冒充真实截图。
"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
base=root/'Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballCastV3StandalonePIE.py'
source=base.read_text(encoding='utf8')
cmd=unreal.SystemLibrary.get_command_line()
stage='IceForm' if '-OrbFormation=IceForm' in cmd else 'FireForm'
data={
 'FireForm':('NS_DBA_FireOrb_Form_Review',{5:'F1_Start',15:'F1_Gather',29:'F1_Shell',41:'F1_OrbFormed'}),
 'IceForm':('NS_DBA_IceBlock_Form_Review',{5:'I1_Start',15:'I1_Condense',29:'I1_Facets',41:'I1_IceFormed'})
}
name,keys=data[stage]
replacements=[
 ('-FireballV3CaptureOwned','-OrbProjectileV1CaptureOwned'),
 ('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review',
  '/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1/Niagara/Systems/'+name),
 ('Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review.uasset',
  'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/OrbProjectile/Review/V1/Niagara/Systems/'+name+'.uasset'),
 ('Saved/Validation/FireballV3P1/NativeRender','Saved/Validation/OrbProjectileV1/NativePIE'),
 ('STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}',
  'STAGES='+repr(keys)),
 ('self.max_frames=46','self.max_frames=42'),
 ('self.camera.capture_scene()\n                self.state="capture2"',
  'if self.frames not in STAGES:return\n                self.camera.capture_scene()\n                self.state="capture2"')
]
for old,new in replacements:
 assert source.count(old)>=1,'截图源模板已变化，不安全: '+old
 source=source.replace(old,new)
# 增加精确分辨率审核，不修改审核Niagara的运行参数。
exec(compile(source,str(base),'exec'),{'__name__':'__main__'})
