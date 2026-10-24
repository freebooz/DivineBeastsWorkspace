# -*- coding: utf-8 -*-
"""P1真实UE5.8对照：读取已登记的公共火球蓄力Niagara，独立审核场景，不修改原有资产。"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
p=root/"Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballCastV3StandalonePIE.py"
src=p.read_text(encoding="utf-8")
old="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review"
new="/DBAPresentationPack_Core/VFX/Library/01_Cast/NS_Fire_Fireball_Charge"
assert src.count(old)==1
src=src.replace(old,new)
oldb="Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review.uasset"
newb="Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Library/01_Cast/NS_Fire_Fireball_Charge.uasset"
assert src.count(oldb)==1
src=src.replace(oldb,newb)
src=src.replace("self.max_frames=46","self.max_frames=40")
src=src.replace('STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}',
                'STAGES={5:"01_CoreStart",14:"02_Charge",27:"03_FlamePlume",39:"04_FirePeak"}')
needle='vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(0,0,145))'
assert src.count(needle)==1
src=src.replace(needle,'vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(30,0,-25))\n        vfx.set_actor_scale3d(unreal.Vector(1.0,1.0,1.0))')
exec(compile(src,str(p),"exec"),{"__name__":"__main__"})
