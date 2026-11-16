# -*- coding: utf-8 -*-
"""《神兽联盟》火球与冰块飞行拖尾V1 UE5.8独立视觉运动审核：
以固定轨迹移动审核NiagaraActor，仅验证Ribbon世界历史位置，不表示服务器真实弹道已接入。
相机/场景均在临时PIE地图，绝不修改Game正式角色或Projectile。
"""
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=ROOT/'Tools/Unreal/VFX/DevelopmentMagic/CaptureFireballCastV3StandalonePIE.py'
source=src.read_text(encoding='utf8')
cmd=unreal.SystemLibrary.get_command_line()
ice='-OrbFlight=IceFlight' in cmd
name='NS_DBA_IceBlock_FlightTrail_Review' if ice else 'NS_DBA_FireOrb_FlightTrail_Review'
keys={5:'I2_Launch',21:'I2_TrailDevelop',43:'I2_Cruise',65:'I2_TrailPeak'} if ice else {5:'F2_Launch',21:'F2_TrailDevelop',43:'F2_Cruise',65:'F2_TrailPeak'}
replace=[
 ('-FireballV3CaptureOwned','-OrbProjectileV1FlightCaptureOwned'),
 ('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review',
  '/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1/Niagara/Systems/'+name),
 ('Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3Review.uasset',
  'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/OrbProjectile/Review/V1/Niagara/Systems/'+name+'.uasset'),
 ('Saved/Validation/FireballV3P1/NativeRender','Saved/Validation/OrbProjectileV1/NativeFlightPIE'),
 ('STAGES={5:"01_HandsIgnite",14:"02_PalmsCharge",27:"03_StreamConverge",41:"04_CoreFormed"}','STAGES='+repr(keys)),
 ('self.max_frames=46','self.max_frames=68'),
 ('mannequin=spawn(unreal.SkeletalMeshActor,"SSLM_P1_Caster",(0,0,0))',
  'mannequin=spawn(unreal.SkeletalMeshActor,"SSLM_P1_Caster",(-990,0,0))'),
 ('vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(0,0,145))',
  'vfx=spawn(unreal.NiagaraActor,"SSLM_P1_CastV3",(-195,0,170))'),
 ('camPos=unreal.Vector(340,-475,250);look=unreal.Vector(29,0,123)',
  'camPos=unreal.Vector(220,-565,270);look=unreal.Vector(-70,0,150)'),
 ('self.comp=actors[0].get_component_by_class(unreal.NiagaraComponent)',
  'self.vfx_actor=actors[0]\n                self.comp=actors[0].get_component_by_class(unreal.NiagaraComponent)'),
 ('self.comp.advance_simulation(1,1/60)',
  'self.vfx_actor.set_actor_location(unreal.Vector(-195+self.frames*4.1,38*math.sin(self.frames*.053),170+8*math.cos(self.frames*.09)),False,False)\n                self.comp.advance_simulation(1,1/60)'),
 ('self.camera.capture_scene()\n                self.state="capture2"',
  'if self.frames not in STAGES:return\n                self.camera.capture_scene()\n                self.state="capture2"'),
 ('import hashlib','import hashlib\nimport math'),
 ('previewUsesProxyHands=True','previewUsesProxyHands=False,scriptedTestTrajectory=True'),
]
for old,new in replace:
 assert source.count(old)>=1,'审核采集模板已变化，拒绝擅自执行 '+old
 source=source.replace(old,new)
exec(compile(source,str(src),'exec'),{'__name__':'__main__'})
