# -*- coding: utf-8 -*-
"""SSLM V2主体外观审核：加入少量真实网格火焰薄片和柔雾，校准新冰晶表面。
这是材质/几何隔离审核，不是Niagara或正式技能。此脚本不创建/运行Niagara作者逻辑。
"""
import unreal,math,json
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2'
assert '-OrbV2ReworkOwned' in unreal.SystemLibrary.get_command_line()
material=unreal.load_asset(BASE+'/Materials/M_DBA_IceCrystal_V2')
assert isinstance(material,unreal.Material)
# 只修改本轮新建冰晶材质，减轻深蓝矿石感；V1材质及正式资产保持原样。
for node in unreal.MaterialEditingLibrary.get_material_expressions(material):
    if isinstance(node,unreal.MaterialExpressionCustom):
        old=node.get_editor_property('code')
        needle='float3(.025,.18,.28),float3(.23,.56,.68)'
        assert needle in old,'新冰晶材质已被其他任务改动，拒绝覆盖'
        revised=old.replace(needle,'float3(.10,.32,.46),float3(.39,.72,.84)')
        revised=revised.replace('saturate(.82+Edge*.16)','saturate(.62+Edge*.32)')
        node.set_editor_property('code',revised)
    elif isinstance(node,unreal.MaterialExpressionScalarParameter):
        if str(node.get_editor_property('parameter_name'))=='InteriorGlow':node.set_editor_property('default_value',.58)
unreal.MaterialEditingLibrary.recompile_material(material)
assert unreal.EditorAssetLibrary.save_loaded_asset(material,False)
# 审核薄片仅验证与主体的外观关系，不记录为运行时粒子系统交付。
source=ROOT/'Tools/Unreal/VFX/DevelopmentMagic/CaptureOrbV2SurfaceProofUE58.py'
text=source.read_text(encoding='utf8')
text=text.replace("OUT=ROOT/'Saved/Validation/OrbProjectileV2/SurfaceProof'","OUT=ROOT/'Saved/Validation/OrbProjectileV2/ArtProofR1'")
text=text.replace('import unreal,time,json,traceback,hashlib','import unreal,time,json,traceback,hashlib,math')
needle='        self.handle=unreal.register_slate_post_tick_callback(self.tick)'
assert text.count(needle)==1
extra='''        self.billboards=[]
        plane=unreal.load_asset('/Engine/BasicShapes/Plane')
        fm=unreal.load_asset(DEST+'/Materials/M_DBA_FireVeil_V2')
        im=unreal.load_asset(DEST+'/Materials/M_DBA_ColdMist_V2')
        for i in range(7):
            theta=i*2.39996323
            p=(math.cos(theta)*22,-72+math.sin(theta)*22,143+math.sin(i*1.47)*10)
            a=spawn(unreal.StaticMeshActor,p,'OrbV2_FlameVeilProof_'+str(i))
            a.static_mesh_component.set_static_mesh(plane);a.static_mesh_component.set_material(0,fm)
            a.set_actor_scale3d(unreal.Vector(.24+.025*(i%3),.48+.04*(i%2),1))
            self.billboards.append(a)
        for i in range(3):
            theta=i*2.39996323
            p=(math.cos(theta)*21,76+math.sin(theta)*23,127+math.sin(theta)*9)
            a=spawn(unreal.StaticMeshActor,p,'OrbV2_MistProof_'+str(i))
            a.static_mesh_component.set_static_mesh(plane);a.static_mesh_component.set_material(0,im)
            a.set_actor_scale3d(unreal.Vector(.60,.56,1))
            self.billboards.append(a)
'''
text=text.replace(needle,extra+needle)
needle='                self.capture.capture_scene();self.last=now;self.phase=\'flush\';return'
assert text.count(needle)==1
text=text.replace(needle,"                for billboard in self.billboards:\n                    billboard.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(unreal.Vector(*pos)-billboard.get_actor_location()),False)\n"+needle)
text=text.replace("'UE5.8 native material and mesh proof, not final Niagara'","'UE5.8 native material/art proof with static review cards; not Niagara or final spell'")
exec(compile(text,str(source),'exec'),{'__name__':'__main__'})
