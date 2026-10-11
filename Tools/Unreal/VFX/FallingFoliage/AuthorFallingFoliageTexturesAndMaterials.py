#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实UE5.8材质/贴图作者脚本，须以官方Editor的PythonScriptPlugin运行。
模式inspect只列计划；apply先导入8张真实PNG，再制作平台统一母材质与项目4个材质实例。
明确拒绝覆盖任何已有资产。仅修改新增目标，不操作玩家正在编辑的UI资源。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCES = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/FallingFoliage/Textures"
MANIFEST = SOURCES / "FallingFoliageSourceManifest.json"
TEXTURES = "/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Textures"
INSTANCES = "/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Materials"
MASTER = "/GamePlatformVFX/Environment/FallingFoliage/Materials"
KINDS = ("Peach", "Maple", "Bamboo", "Ginkgo")
MODE = os.environ.get("FOLIAGE_AUTHOR_MODE","inspect").lower()


def verify_sources():
    if not MANIFEST.exists():
        raise FileNotFoundError("缺少源纹理manifest")
    data=json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data["schemaVersion"]!=1 or len(data["textures"])!=8:
        raise ValueError("源纹理清单格式不兼容")
    verified=[]
    for spec in data["textures"]:
        path=SOURCES / spec["name"]
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=spec["sha256"]:
            raise RuntimeError("源纹理哈希不一致："+str(path))
        verified.append((path,spec))
    return verified


def imported_path(kind: str, kind_type: str) -> str:
    return f"{TEXTURES}/T_DBA_Foliage_{kind}_{kind_type}"


def import_png_assets(assets, asset_tools, unreal, verified):
    missing=[]
    for path,spec in verified:
        name=path.stem.removesuffix("_Source")
        target=TEXTURES+"/"+name
        if assets.does_asset_exist(target):
            raise FileExistsError("目标贴图已有用户资源，禁止覆盖："+target)
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(path))
        task.set_editor_property("destination_path",TEXTURES)
        task.set_editor_property("destination_name",name)
        task.set_editor_property("automated",True)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("save",False)
        missing.append((spec,task,target))
    if not assets.does_directory_exist(TEXTURES):
        assets.make_directory(TEXTURES)
    asset_tools.import_asset_tasks([t for _,t,_ in missing])
    for spec,task,target in missing:
        imported=list(task.get_editor_property("imported_object_paths"))
        texture=assets.load_asset(target)
        if not imported or not isinstance(texture,unreal.Texture2D):
            raise RuntimeError("未生成真实Texture2D："+target)
        texture.set_editor_property("srgb",bool(spec["sourceSRGB"]))
        texture.set_editor_property("compression_settings",
            unreal.TextureCompressionSettings.TC_NORMALMAP if spec["type"]=="Normal"
            else unreal.TextureCompressionSettings.TC_DEFAULT)
        if not assets.save_loaded_asset(texture,only_if_is_dirty=False):
            raise RuntimeError("UE保存纹理失败："+target)
        if not isinstance(assets.load_asset(target),unreal.Texture2D):
            raise RuntimeError("保存后纹理回读失败："+target)
        print("FOLIAGE_UE_TEXTURE_SAVED",target)


def author_materials(assets,asset_tools,unreal):
    lib=unreal.MaterialEditingLibrary
    master_path=MASTER+"/M_GP_FallingFoliage_Master"
    if assets.does_asset_exist(master_path):
        raise FileExistsError("通用母材质已有资源，不覆盖："+master_path)
    for kind in KINDS:
        path=INSTANCES+f"/MI_DBA_Foliage_{kind}"
        if assets.does_asset_exist(path):
            raise FileExistsError("项目材质实例已有资源，不覆盖："+path)
        for typ in ("Albedo","Normal"):
            tex_path=imported_path(kind,typ)
            if not isinstance(assets.load_asset(tex_path),unreal.Texture2D):
                raise FileNotFoundError("缺失已导入纹理："+tex_path)
    if not assets.does_directory_exist(MASTER):
        assets.make_directory(MASTER)
    if not assets.does_directory_exist(INSTANCES):
        assets.make_directory(INSTANCES)

    master=asset_tools.create_asset("M_GP_FallingFoliage_Master",MASTER,unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(master,unreal.Material):
        raise RuntimeError("未获得真实UMaterial")
    master.set_editor_property("blend_mode",unreal.BlendMode.BLEND_MASKED)
    master.set_editor_property("shading_model",unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    master.set_editor_property("two_sided",True)
    # Niagara Sprite材质必须显式开启使用标志，否则真实运行会回退默认材质。
    master.set_editor_property("used_with_niagara_sprites",True)
    master.set_editor_property("opacity_mask_clip_value",0.20)
    def node(cls,x,y):
        return lib.create_material_expression(master,cls,x,y)
    def connect(source,out,dest,input_name):
        if not lib.connect_material_expressions(source,out,dest,input_name):
            raise RuntimeError(f"材质节点连接失败：{source.get_name()} {input_name}")
    def prop(source,out,mat_property):
        if not lib.connect_material_property(source,out,mat_property):
            raise RuntimeError("材质输出连接失败："+str(mat_property))

    color=node(unreal.MaterialExpressionTextureSampleParameter2D,-900,-220)
    color.set_editor_property("parameter_name",unreal.Name("LeafAlbedo"))
    color.set_editor_property("texture",assets.load_asset(imported_path("Peach","Albedo")))
    # 平台母材质不得固定硬引用任何项目层素材，改用引擎默认源纹理。
    default_color=assets.load_asset("/Engine/EngineResources/DefaultTexture")
    if default_color is None:
        raise RuntimeError("引擎DefaultTexture未找到；禁止平台母材质引用项目贴图")
    color.set_editor_property("texture",default_color)
    particle=node(unreal.MaterialExpressionParticleColor,-900,140)
    multiply=node(unreal.MaterialExpressionMultiply,-530,-160)
    connect(color,"RGB",multiply,"A")
    connect(particle,"RGB",multiply,"B")
    prop(multiply,"",unreal.MaterialProperty.MP_BASE_COLOR)

    mask_mul=node(unreal.MaterialExpressionMultiply,-520,240)
    connect(color,"A",mask_mul,"A")
    connect(particle,"A",mask_mul,"B")
    prop(mask_mul,"",unreal.MaterialProperty.MP_OPACITY_MASK)

    normal=node(unreal.MaterialExpressionTextureSampleParameter2D,-900,530)
    normal.set_editor_property("parameter_name",unreal.Name("LeafNormal"))
    default_normal=assets.load_asset("/Engine/EngineMaterials/DefaultNormal")
    if default_normal is None:
        raise RuntimeError("引擎DefaultNormal未找到；不允许引用项目资源")
    normal.set_editor_property("texture",default_normal)
    normal.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    prop(normal,"RGB",unreal.MaterialProperty.MP_NORMAL)

    rough=node(unreal.MaterialExpressionScalarParameter,-560,540)
    rough.set_editor_property("parameter_name",unreal.Name("LeafRoughness"))
    rough.set_editor_property("default_value",0.72)
    prop(rough,"",unreal.MaterialProperty.MP_ROUGHNESS)
    spec=node(unreal.MaterialExpressionScalarParameter,-560,760)
    spec.set_editor_property("parameter_name",unreal.Name("LeafSpecular"))
    spec.set_editor_property("default_value",0.28)
    prop(spec,"",unreal.MaterialProperty.MP_SPECULAR)

    errors=list(lib.recompile_material(master))
    if any("error" in str(e).lower() for e in errors):
        raise RuntimeError("通用母材质shader编译错误："+str(errors))
    if not assets.save_loaded_asset(master,only_if_is_dirty=False):
        raise RuntimeError("通用母材质保存失败")
    print("FOLIAGE_UE_MASTER_SAVED",master_path)

    for kind in KINDS:
        instance=asset_tools.create_asset("MI_DBA_Foliage_"+kind,INSTANCES,
            unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(instance,unreal.MaterialInstanceConstant):
            raise RuntimeError("未生成真实材质实例："+kind)
        lib.set_material_instance_parent(instance,master)
        for typ,param in (("Albedo","LeafAlbedo"),("Normal","LeafNormal")):
            lib.set_material_instance_texture_parameter_value(instance,
                unreal.Name(param),assets.load_asset(imported_path(kind,typ)))
        # 纸质/花瓣表面高粗糙度，减少夸张的镜面反射。
        lib.set_material_instance_scalar_parameter_value(instance,unreal.Name("LeafRoughness"),
             {"Peach":.78,"Maple":.69,"Bamboo":.58,"Ginkgo":.74}[kind])
        if not assets.save_loaded_asset(instance,only_if_is_dirty=False):
            raise RuntimeError("材质实例保存失败："+kind)
        if not isinstance(assets.load_asset(INSTANCES+"/MI_DBA_Foliage_"+kind),
                          unreal.MaterialInstanceConstant):
            raise RuntimeError("材质实例回读异常："+kind)
        print("FOLIAGE_UE_MATERIAL_INSTANCE_SAVED",kind)


def main():
    verified=verify_sources()
    for path,spec in verified:
        print("SOURCE_OK",spec["type"],spec["species"],path.name)
    if MODE=="inspect":
        print("FOLIAGE_ASSET_INSPECT_ONLY")
        return
    if MODE not in ("apply","import","material"):
        raise ValueError("FOLIAGE_AUTHOR_MODE只能inspect/apply/import/material")
    import unreal  # type: ignore
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("编辑器未载入《神兽联盟》正式项目："+project)
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("资产作者工具必须使用UE5.8")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    if MODE in ("apply","import"):
        import_png_assets(assets,tools,unreal,verified)
    if MODE in ("apply","material"):
        author_materials(assets,tools,unreal)
    print("FOLIAGE_UE_MATERIAL_STAGE_COMPLETED",MODE)


if __name__=="__main__":
    main()
