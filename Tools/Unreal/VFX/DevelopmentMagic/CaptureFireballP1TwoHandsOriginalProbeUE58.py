# -*- coding: utf-8 -*-
"""临时诊断：同镜头和左右手坐标，改用未修改的 FireMage 资源，判定新材质或坐标问题。只读源资产。"""
import unreal
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
source=root/"Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballP1TwoHandsUE58.py"
txt=source.read_text(encoding="utf-8")
old1="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/NS_DBA_Fireball_P1_01_HandsIgnite_Review"
old2="/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/P1Hands/Niagara/NS_DBA_Fireball_P1_02_PalmsCharge_Review"
original="/DBAPresentationPack_Core/VFX/Library/Game/FireMage/Niagara/NS_PY_Charge"
assert txt.count(old1)==1 and txt.count(old2)==1
txt=txt.replace(old1,original).replace(old2,original)
exec(compile(txt,str(source),"exec"),{"__name__":"__main__"})
