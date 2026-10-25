# -*- coding: utf-8 -*-
"""《神兽联盟》火球/冰块成形+飞行拖尾四阶段资源源头核查，仅只读UE5.8 AssetRegistry，不保存资产。
输出项目审核复用基准，列举真实Mesh/Sprite/Ribbon发射器材质。"""
import unreal,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
b="/DBAPresentationPack_Core/VFX"
paths={
"FireFormPrimary":b+"/Library/01_Cast/NS_Fire_Fireball_Charge",
"FireFormAlternate":b+"/Library/Game/FireMage/Niagara/NS_PY_Charge",
"FireFlight":b+"/DevelopmentMagic/Niagara/NS_DBA_DevFireballFlight",
"IceForm":b+"/IceArrow/Niagara/NS_DBA_IceArrow_Cast",
"IceFlight":b+"/IceArrow/Niagara/NS_DBA_IceArrow_Flight",
"IceFlightReview":b+"/IceArrow/Niagara/Review/NS_DBA_IceArrow_Flight_A",
"IceMeshLance":b+"/IceArrow/Meshes/SM_DBA_IceArrow_CrystalLance",
"IceMeshShard":b+"/IceArrow/Meshes/SM_DBA_IceArrow_CrystalShard",
"FireRibbonMaterial":b+"/Library/00_Core/Materials/Fire/M_Fire_Ribbon",
"FrostRibbonMaterial":b+"/IceArrow/Materials/MI_DBA_IceArrow_Trail",
"ProjectileTrailEffect":b+"/Library/00_Core/EffectTypes/ProjectileTrail/ET_VFX_ProjectileTrail"
}
types=[("Sprite","NiagaraSpriteRendererProperties"),("Mesh","NiagaraMeshRendererProperties"),("Ribbon","NiagaraRibbonRendererProperties")]
report={"engine":unreal.SystemLibrary.get_engine_version(),"project":unreal.Paths.get_project_file_path(),"sources":{}}
for k,p in paths.items():
    a=unreal.load_asset(p)
    x={"path":p,"exists":a is not None,"class":a.get_class().get_name() if a else None,"renderers":[],"moduleCalls":[]}
    if isinstance(a,unreal.NiagaraSystem):
        for label,typ in types:
            cls=getattr(unreal,typ,None)
            if cls:
                for o in unreal.ObjectIterator(cls):
                    if o.get_path_name().startswith(a.get_path_name()+":"):
                        d={"type":label,"path":o.get_path_name()}
                        for prop in ["material","mesh","ribbon_width_binding","facing_mode"]:
                            try:
                                z=o.get_editor_property(prop)
                                d[prop]=z.get_path_name() if isinstance(z,unreal.Object) else str(z)[:220]
                            except:pass
                        x["renderers"].append(d)
        for o in unreal.ObjectIterator(unreal.NiagaraNodeFunctionCall):
            if o.get_path_name().startswith(a.get_path_name()+":"):
                f=o.get_editor_property("function_script")
                x["moduleCalls"].append(f.get_path_name() if f else "")
    report["sources"][k]=x
dest=root/"Saved/Validation/OrbProjectileV1/SourceInventory.json"
dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
for k,v in report["sources"].items():
    print("SOURCE",k,v["exists"],v["class"],"renderers",len(v["renderers"]))
print("SOURCE_INVENTORY",dest)
