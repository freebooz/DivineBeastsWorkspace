# -*- coding: utf-8 -*-
"""火球V3 P1高质量火焰材质复用审核：不修改已经存在的独立P1审核主体。"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
source=root/"Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballCastV3StandalonePIE.py"
body=source.read_text(encoding="utf-8")
old="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review"
new="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3RefCharge"
assert body.count(old)==1,"预期恰好一个审核系统路径"
body=body.replace(old,new)
oldbinary="NS_DBA_FireballCast_V3Review.uasset"
assert body.count(oldbinary)==1
body=body.replace(oldbinary,"NS_DBA_FireballCast_V3RefCharge.uasset")
body=body.replace('STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}',
                  'STAGES={5:"01_FireAwakens",14:"02_FlameGathers",27:"03_FlameSwirls",41:"04_ChargePlume"}')
exec(compile(body,str(source),"exec"),{"__name__":"__main__"})
