# -*- coding: utf-8 -*-
"""P1-02 真实Niagara单系统固定位置诊断，禁止以此作为双掌通过截图。
复用已成功拍到公共火焰的同一 UE5.8 审核夹具，仅将源替换为P1-02审核Niagara。
"""
import unreal
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
p=root/"Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballCastV3StandalonePIE.py"
s=p.read_text(encoding="utf8")
old="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review"
new="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/NS_DBA_Fireball_P1_02_PalmsCharge_Review"
assert s.count(old)==1
s=s.replace(old,new)
old_b="Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review.uasset"
new_b="Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/NS_DBA_Fireball_P1_02_PalmsCharge_Review.uasset"
assert old_b in s
s=s.replace(old_b,new_b)
s=s.replace('vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(0,0,145))','vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(30,0,-25))')
s=s.replace('STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}',
            'STAGES={9:"Probe_early",23:"Probe_charge"}')
s=s.replace('self.max_frames=46','self.max_frames=24')
s=s.replace('Saved/Validation/FireballV3P1/NativeRender','Saved/Validation/FireballV3P1/StaticVisibilityProbe')
exec(compile(s,str(p),"exec"),{"__name__":"__main__"})
