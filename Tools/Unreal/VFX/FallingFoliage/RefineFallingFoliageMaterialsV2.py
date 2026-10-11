# -*- coding: utf-8 -*-
"""UE5.8已存在的自然飘落纹理/母材质/材质实例V2增量重建。
仅作用于四类项目SourceArt对应的8张Texture2D及1个平台母材质/4个实例。
真实UE Editor import/recompile/save；事前备份旧.uasset至Saved，不伪造二进制。
"""
from __future__ import annotations
import json
import hashlib
import os
import shutil
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/FallingFoliage/Textures"
MANIFEST=SOURCE/"FallingFoliageSourceManifest.json"
PFX="/DBAPresentationPack_Core/VFX/Environment/FallingFoliage"
MASTER="/GamePlatformVFX/Environment/FallingFoliage/Materials/M_GP_FallingFoliage_Master"
BACKUP=ROOT/"Saved/Validation/FallingFoliage/BeautyV2UEBackup"
REPORT=ROOT/"Saved/Validation/FallingFoliage/BeautyV2UEImportResult.json"
SPECIES=("Peach","Maple","Bamboo","Ginkgo")
ASSETS=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
result={"engine":str(unreal.SystemLibrary.get_engine_version()),
        "project":str(unreal.Paths.get_project_file_path()),
        "textures":[],"material":{},"instances":[],"errors":[]}

def ue_load(path,cls):
    obj=ASSETS.load_asset(path)
    if obj is None or not isinstance(obj,cls):
        raise RuntimeError(f"真实UE资产丢失/类型不匹配：{path}，期待{cls}")
    return obj

def backed_up(path):
    realpkg=str(path).split(".")[0]
    mount,base=realpkg.strip("/").split("/",1)
    if mount=="GamePlatformVFX":
        base_dir=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content"
    elif mount=="DBAPresentationPack_Core":
        base_dir=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content"
    else:
        raise RuntimeError("拒绝非本任务挂载点："+mount)
    disk=base_dir/(base+".uasset")
    if not disk.exists():
        raise FileNotFoundError(str(disk))
    destination=BACKUP/mount/(base+".uasset")
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists():
        shutil.copy2(disk,destination)
    return disk

def import_sources():
    specs=json.loads(MANIFEST.read_text(encoding="utf-8"))["textures"]
    if len(specs)!=8:
        raise RuntimeError("V2源纹理应为8张")
    # 先验证所有哈希、资产类及回滚备份，避免导入至一半才发现资源不一致。
    for spec in specs:
        if spec["species"] not in SPECIES: raise RuntimeError("非法叶片名称")
        f=SOURCE/spec["name"]
        if hashlib.sha256(f.read_bytes()).hexdigest()!=spec["sha256"]:
            raise RuntimeError("SHA-256与Manifest不一致："+str(f))
        target=PFX+"/Textures/"+f.stem.removesuffix("_Source")
        ue_load(target,unreal.Texture2D)
        backed_up(target)
    for spec in specs:
        source=SOURCE/spec["name"]
        name=source.stem.removesuffix("_Source")
        target=PFX+"/Textures/"+name
        task=unreal.AssetImportTask()
        for field,value in (("filename",str(source)),("destination_path",PFX+"/Textures"),
                            ("destination_name",name),("automated",True),
                            ("replace_existing",True),("save",False)):
            task.set_editor_property(field,value)
        TOOLS.import_asset_tasks([task])
        actual=ue_load(target,unreal.Texture2D)
        actual.set_editor_property("srgb",bool(spec["sourceSRGB"]))
        actual.set_editor_property("compression_settings",
            unreal.TextureCompressionSettings.TC_NORMALMAP if spec["type"]=="Normal"
            else unreal.TextureCompressionSettings.TC_DEFAULT)
        if not ASSETS.save_loaded_asset(actual,only_if_is_dirty=False):
            raise RuntimeError("纹理真实保存失败："+target)
        # 实例引用不会因替换同路径Texture2D而改变。
        result["textures"].append({"path":target,"sourceHash":spec["sha256"],"saved":True})
        print("BEAUTY_V2_TEXTURE_REIMPORTED",name,flush=True)

def material():
    m=ue_load(MASTER,unreal.Material)
    backed_up(MASTER)
    m.set_editor_property("two_sided",True)
    m.set_editor_property("used_with_niagara_sprites",True)
    m.set_editor_property("blend_mode",unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("opacity_mask_clip_value",0.18)
    enum_names=[x for x in dir(unreal.MaterialShadingModel) if "FOLIAGE" in x.upper() or "SUBSURFACE" in x.upper()]
    result["available_shading_enums"]=enum_names
    desired=None
    for x in enum_names:
        if "FOLIAGE" in x.upper():
            desired=getattr(unreal.MaterialShadingModel,x)
            result["chosen_shading_enum"]=x
            break
    if desired is None:
        for x in enum_names:
            if "SUBSURFACE" in x.upper() and "PROFILE" not in x.upper():
                desired=getattr(unreal.MaterialShadingModel,x)
                result["chosen_shading_enum"]=x
                break
    if desired is not None:
        m.set_editor_property("shading_model",desired)
    else:
        result["errors"].append("没有可反射的Foliage/Subsurface shading_model，材质维持DefaultLit，透光依靠纹理")
    expr=LIB.get_material_expressions(m)
    albedo=None
    transmission=None
    existing_multiply=None
    for e in expr:
        try: param=str(e.get_editor_property("parameter_name"))
        except Exception: param=""
        if param=="LeafAlbedo": albedo=e
        if param=="LeafTransmission": transmission=e
        if e.get_class().get_name()=="MaterialExpressionMultiply":
            if e.get_name().startswith("MaterialExpressionMultiply_FoliageTransmission"):
                existing_multiply=e
    if albedo is None:
        raise RuntimeError("已有母材质缺少稳定LeafAlbedo参数，拒绝破坏纹理合同")
    if transmission is None:
        transmission=LIB.create_material_expression(m,unreal.MaterialExpressionScalarParameter,25,780)
        transmission.set_editor_property("parameter_name",unreal.Name("LeafTransmission"))
        transmission.set_editor_property("default_value",0.44)
    if existing_multiply is None:
        existing_multiply=LIB.create_material_expression(m,unreal.MaterialExpressionMultiply,265,700)
        # 存在同名V2节点下次以连接来源检测；由固定参数与类型共同认定幂等。
    if not LIB.connect_material_expressions(albedo,"RGB",existing_multiply,"A"):
        raise RuntimeError("LeafAlbedo -> Translucency multiply失败")
    if not LIB.connect_material_expressions(transmission,"",existing_multiply,"B"):
        raise RuntimeError("LeafTransmission -> Translucency multiply失败")
    if desired is not None:
        if not LIB.connect_material_property(existing_multiply,"",unreal.MaterialProperty.MP_SUBSURFACE_COLOR):
            raise RuntimeError("连接次表面颜色失败")
    compile_result=LIB.recompile_material(m)
    if compile_result and any("error" in str(x).lower() for x in compile_result):
        raise RuntimeError("母材质重新编译失败："+str(compile_result))
    if not ASSETS.save_loaded_asset(m,only_if_is_dirty=False):
        raise RuntimeError("母材质保存失败")
    result["material"]={"path":MASTER,"shading":str(m.get_editor_property("shading_model")),
                       "doubleSided":True,"blend":"Masked","clip":0.18,
                       "transmission":0.44,"compile_result":str(compile_result)}
    for species in SPECIES:
        path=PFX+"/Materials/MI_DBA_Foliage_"+species
        inst=ue_load(path,unreal.MaterialInstanceConstant)
        backed_up(path)
        if species=="Peach":
            rough,specular,through=0.81,0.16,0.56
        elif species=="Maple":
            rough,specular,through=0.79,0.20,0.30
        elif species=="Bamboo":
            rough,specular,through=0.72,0.20,0.35
        else:
            rough,specular,through=0.82,0.18,0.39
        LIB.set_material_instance_parent(inst,m)
        LIB.set_material_instance_scalar_parameter_value(inst,"LeafRoughness",rough)
        LIB.set_material_instance_scalar_parameter_value(inst,"LeafSpecular",specular)
        LIB.set_material_instance_scalar_parameter_value(inst,"LeafTransmission",through)
        for typ,param in (("Albedo","LeafAlbedo"),("Normal","LeafNormal")):
            tex=ue_load(PFX+"/Textures/T_DBA_Foliage_"+species+"_"+typ,unreal.Texture2D)
            LIB.set_material_instance_texture_parameter_value(inst,param,tex)
        if not ASSETS.save_loaded_asset(inst,only_if_is_dirty=False):
            raise RuntimeError("材质实例未保存："+path)
        result["instances"].append({"path":path,"roughness":rough,
            "specular":specular,"transmission":through,"saved":True})
        print("BEAUTY_V2_MATERIAL_INSTANCE_SAVED",species,flush=True)

def main():
    if not result["project"].replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("必须在正式神兽联盟工程执行")
    if not result["engine"].startswith("5.8."):
        raise RuntimeError("需要UE5.8")
    BACKUP.mkdir(parents=True,exist_ok=True)
    try:
        import_sources()
        material()
        result["success"]=True
    except Exception as ex:
        result["success"]=False
        result["errors"].append(str(ex))
        raise
    finally:
        REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
        print("BEAUTY_V2_UE_REPORT",REPORT,"success",result.get("success"),flush=True)

if __name__=="__main__":
    main()
