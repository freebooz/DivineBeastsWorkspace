#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟CliffRock01（青灰色绝壁）完整4K PBR的UE5.8原生资源创建及回读。

归属：第三层DivineBeasts/DBAWorldPack_Village内容包；只创建独立Realism/CliffRock01。
必须先有Poly Haven官方marble_cliff_05同源扫描四通道+青灰基础色派生，
SHA256一致后才能创建Texture2D、PBR母材质、预览MI及十层Rock项目MI。
法线采用DirectX约定不重复翻转；ORM采用R遮蔽/G粗糙度/B金属度；
原始16位Height单独导入备将来高度混合，不声称已制作Nanite位移或三向投射。
不覆盖林地Rock01、十层基础UMaterial及正式Village地图。
CLIFFROCK_UE_MODE=inspect/apply/verify；UE真实保存、Shader、对象类别/继承/纹理指针
均需回读，报错时禁止虚称已经完成。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
        "SourceArt/Terrain/PBR/CliffRock01")
MANIFEST=SOURCE/"CliffRock01_SourceManifest.json"
GRADE=SOURCE/"CliffRock01_ColorGradeManifest.json"
ROOT_ASSET="/DBAWorldPack_Village/Terrain/Realism/CliffRock01"
TEXTURE_ROOT=ROOT_ASSET+"/Textures"
MATERIAL_ROOT=ROOT_ASSET+"/Materials"
TEN_LAYER_MASTER="/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10"
MATERIAL=MATERIAL_ROOT+"/M_DBA_CliffRock01_PBRReview"
REVIEW_MI=MATERIAL_ROOT+"/MI_DBA_CliffRock01_PBRReview"
LANDSCAPE_MI=MATERIAL_ROOT+"/MI_DBA_Terrain_CliffRock01"
ROLE_ASSETS={
    "BaseColor":TEXTURE_ROOT+"/T_DBA_CliffRock01_BaseColor_BlueGray",
    "NormalDX":TEXTURE_ROOT+"/T_DBA_CliffRock01_NormalDX",
    "ORM":TEXTURE_ROOT+"/T_DBA_CliffRock01_ORM",
    "Height":TEXTURE_ROOT+"/T_DBA_CliffRock01_Height",
}
MODE=os.getenv("CLIFFROCK_UE_MODE","inspect").strip().lower()


def signed_sources()->dict[str,Path]:
    if not MANIFEST.is_file() or not GRADE.is_file():
        raise FileNotFoundError("缺少青灰CliffRock01官方扫描及色彩变体签名清单")
    doc=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    graded=json.loads(GRADE.read_text(encoding="utf-8-sig"))
    if (doc.get("schemaVersion")!=1 or doc.get("sourceId")!="marble_cliff_05" or
            doc.get("sourceResolution")!="4k" or doc.get("license")!="CC0 1.0" or
            doc.get("productionAssetApproved") is not False):
        raise RuntimeError("CliffRock扫描许可/来源身份/人工签收状态非法")
    if (graded.get("schemaVersion")!=1 or
            graded.get("sourceId")!="marble_cliff_05" or
            graded.get("normalOrmHeightSourceUnchanged") is not True or
            graded.get("productionAssetApproved") is not False):
        raise RuntimeError("青灰色派生不能影响原扫描法线/ORM/高度")
    originals={}
    for item in doc["maps"]:
        role=item["role"]
        if role in originals or role not in ROLE_ASSETS:
            raise RuntimeError("项目Cliff PBR源图角色重复或非法："+role)
        path=(ROOT/item["localPath"]).resolve()
        if not path.is_relative_to(SOURCE.resolve()) or not path.is_file():
            raise FileNotFoundError("原始扫描文件缺失或不属世界内容包："+str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest()!=item["sha256"]:
            raise RuntimeError("Cliff原始同源扫描SHA256改变："+role)
        originals[role]=path
    if len(originals)!=4:
        raise RuntimeError("必须有四张原始扫描图，不允许用概念图代替Normal")
    graded_path=(ROOT/graded["gradedSourcePath"]).resolve()
    if not graded_path.is_relative_to(SOURCE.resolve()) or not graded_path.is_file():
        raise FileNotFoundError("青灰色基础色变体不存在")
    if hashlib.sha256(graded_path.read_bytes()).hexdigest()!=graded["gradedBaseColorSHA256"]:
        raise RuntimeError("冷色基础色派生签名错误，不允许导入UE")
    if graded["originalBaseColorSHA256"]!=next(x["sha256"] for x in doc["maps"]
                                                if x["role"]=="BaseColor"):
        raise RuntimeError("原始BaseColor签名与可复现青灰派生关系不符")
    originals["BaseColor"]=graded_path
    return originals


def main()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("CLIFFROCK_UE_MODE仅允许inspect/apply/verify")
    sources=signed_sources()
    print("CLIFFROCK_UE_AUTHOR_PLAN",MODE,TEXTURE_ROOT,MATERIAL,LANDSCAPE_MI)
    if MODE=="inspect":
        return

    try:
        import unreal # type: ignore
    except ImportError as e:
        raise RuntimeError("只有UE5.8原生编辑器能够创建真正.uasset") from e
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("未加载正式DivineBeastsWorkspace项目")
    library=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    edit=unreal.MaterialEditingLibrary
    parent=library.load_asset(TEN_LAYER_MASTER)
    if not isinstance(parent,unreal.Material):
        raise TypeError("10层Landscape既有母材质不可用，拒绝创建孤立MI")
    m_existing=library.does_asset_exist(MATERIAL)
    for package in ROLE_ASSETS.values():
        if library.does_asset_exist(package) and not isinstance(
                library.load_asset(package),unreal.Texture2D):
            raise TypeError("已有同名包不是Texture2D，禁止覆盖："+package)
    for package,cls in ((MATERIAL,unreal.Material),
                        (REVIEW_MI,unreal.MaterialInstanceConstant),
                        (LANDSCAPE_MI,unreal.MaterialInstanceConstant)):
        if library.does_asset_exist(package) and not isinstance(library.load_asset(package),cls):
            raise TypeError("材质候选同名资产存在类别冲突："+package)

    if MODE=="apply":
        pending=[]
        for role,src in sources.items():
            package=ROLE_ASSETS[role]
            if library.does_asset_exist(package):
                print("CLIFFROCK_TEXTURE_EXISTS_REUSE",package)
                continue
            folder,name=package.rsplit("/",1)
            if not library.does_directory_exist(folder):
                library.make_directory(folder)
            task=unreal.AssetImportTask()
            task.set_editor_property("filename",str(src))
            task.set_editor_property("destination_path",folder)
            task.set_editor_property("destination_name",name)
            task.set_editor_property("automated",True)
            task.set_editor_property("replace_existing",False)
            task.set_editor_property("save",False)
            pending.append((role,task))
        if pending:
            tools.import_asset_tasks([task for role,task in pending])
        compression={
            "BaseColor":unreal.TextureCompressionSettings.TC_DEFAULT,
            "NormalDX":unreal.TextureCompressionSettings.TC_NORMALMAP,
            "ORM":unreal.TextureCompressionSettings.TC_MASKS,
            "Height":unreal.TextureCompressionSettings.TC_GRAYSCALE,
        }
        for role,task in pending:
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE官方导入器没有创建Cliff纹理："+role)
            obj=library.load_asset(ROLE_ASSETS[role])
            if not isinstance(obj,unreal.Texture2D):
                raise TypeError("导入结果不是UE Texture2D："+role)
            obj.set_editor_property("srgb",role=="BaseColor")
            obj.set_editor_property("compression_settings",compression[role])
            if role=="NormalDX":
                obj.set_editor_property("flip_green_channel",False)
            if not library.save_loaded_asset(obj,only_if_is_dirty=False):
                raise RuntimeError("Cliff纹理原生保存失败："+role)
            print("CLIFFROCK_UE_TEXTURE_SAVED",ROLE_ASSETS[role])
    textures={}
    for role,package in ROLE_ASSETS.items():
        asset=library.load_asset(package)
        if not isinstance(asset,unreal.Texture2D):
            raise RuntimeError("Cliff四张UE纹理缺失或类型错误："+package)
        if bool(asset.get_editor_property("srgb"))!=(role=="BaseColor"):
            raise RuntimeError("Cliff纹理sRGB设置错误："+role)
        x=asset.blueprint_get_size_x()
        y=asset.blueprint_get_size_y()
        if x!=4096 or y!=4096:
            raise RuntimeError("Cliff UE贴图不是4096x4096："+role)
        textures[role]=asset
        print("CLIFFROCK_UE_TEXTURE_REFLECTED",role,x,y,package)

    if MODE=="apply" and not m_existing:
        folder,name=MATERIAL.rsplit("/",1)
        if not library.does_directory_exist(folder):
            library.make_directory(folder)
        mat=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(mat,unreal.Material):
            raise RuntimeError("UnrealEditor创建Cliff PBR Material失败")
        def expr(cls,x,y):
            e=edit.create_material_expression(mat,cls,int(x),int(y))
            if not e:
                raise RuntimeError("Cliff PBR真实材质节点创建失败："+str(cls))
            return e
        def connect(left,right,pin,out=""):
            ok=edit.connect_material_expressions(left,out,right,pin)
            if not ok and pin=="Coordinates":
                ok=edit.connect_material_expressions(left,out,right,"UVs")
            if not ok:
                raise RuntimeError(f"Cliff PBR连线失败 {left.get_name()}→{right.get_name()}:{pin}")
        uv=expr(unreal.MaterialExpressionTextureCoordinate,-1450,0)
        tile=expr(unreal.MaterialExpressionScalarParameter,-1470,230)
        tile.set_editor_property("parameter_name",unreal.Name("CliffRockUVTiling"))
        tile.set_editor_property("default_value",1.0)
        multiply=expr(unreal.MaterialExpressionMultiply,-1120,75)
        connect(uv,multiply,"A");connect(tile,multiply,"B")
        def tex_node(role,name,x,y):
            sample=expr(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
            sample.set_editor_property("parameter_name",unreal.Name(name))
            sample.set_editor_property("texture",textures[role])
            if role=="NormalDX":
                sample.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            if role=="ORM":
                sample.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            connect(multiply,sample,"Coordinates")
            return sample
        base=tex_node("BaseColor","CliffRockBaseColor",-820,-640)
        normal=tex_node("NormalDX","CliffRockNormalDX",-810,-80)
        orm=tex_node("ORM","CliffRockORM",-820,520)
        tint=expr(unreal.MaterialExpressionVectorParameter,-530,-790)
        tint.set_editor_property("parameter_name",unreal.Name("CliffRockColorTint"))
        tint.set_editor_property("default_value",unreal.LinearColor(1,1,1,1))
        modulated=expr(unreal.MaterialExpressionMultiply,-310,-560)
        connect(base,modulated,"A","RGB")
        connect(tint,modulated,"B")
        roughness_boost=expr(unreal.MaterialExpressionScalarParameter,-570,870)
        roughness_boost.set_editor_property("parameter_name",unreal.Name("CliffRockRoughnessScale"))
        roughness_boost.set_editor_property("default_value",1.0)
        multiply_rough=expr(unreal.MaterialExpressionMultiply,-310,620)
        connect(orm,multiply_rough,"A","G")
        connect(roughness_boost,multiply_rough,"B")
        material_edges=(
            (modulated,"",unreal.MaterialProperty.MP_BASE_COLOR),
            (normal,"RGB",unreal.MaterialProperty.MP_NORMAL),
            (orm,"R",unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
            (multiply_rough,"",unreal.MaterialProperty.MP_ROUGHNESS),
            (orm,"B",unreal.MaterialProperty.MP_METALLIC),
        )
        for node,channel,property_id in material_edges:
            if not edit.connect_material_property(node,channel,property_id):
                raise RuntimeError("Cliff PBR Shader物理输出连线失败："+str(property_id))
        node_count=edit.get_num_material_expressions(mat)
        if node_count<10:
            raise RuntimeError("Cliff PBR母材质节点数不足："+str(node_count))
        compile_results=edit.recompile_material(mat) or []
        for message in compile_results:
            print("CLIFFROCK_UE_SHADER_DIAGNOSTIC",str(message))
            if "error" in str(message).lower():
                raise RuntimeError("UE Cliff岩壁Shader编译错误")
        if not library.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("Cliff PBR Material保存失败")
        print("CLIFFROCK_UE_PBR_MATERIAL_SAVED",MATERIAL,"nodes",node_count)

    configs=(
        (REVIEW_MI,MATERIAL,(
            ("CliffRockBaseColor","BaseColor"),
            ("CliffRockNormalDX","NormalDX"),
            ("CliffRockORM","ORM"),
        )),
        (LANDSCAPE_MI,TEN_LAYER_MASTER,(
            ("TerrainRockBaseColor","BaseColor"),
            ("TerrainRockNormal","NormalDX"),
            ("TerrainRockORM","ORM"),
        )),
    )
    if MODE=="apply":
        for package,expected_parent,params in configs:
            if library.does_asset_exist(package):
                print("CLIFFROCK_MI_EXISTS_NO_OVERWRITE",package)
                continue
            folder,name=package.rsplit("/",1)
            mi=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,
                                  unreal.MaterialInstanceConstantFactoryNew())
            if not isinstance(mi,unreal.MaterialInstanceConstant):
                raise RuntimeError("UE没有生成真实Cliff MaterialInstance："+package)
            edit.set_material_instance_parent(mi,library.load_asset(expected_parent))
            for param,role in params:
                edit.set_material_instance_texture_parameter_value(mi,param,textures[role])
            if package==REVIEW_MI:
                edit.set_material_instance_scalar_parameter_value(mi,"CliffRockUVTiling",1.0)
                edit.set_material_instance_scalar_parameter_value(mi,"CliffRockRoughnessScale",1.0)
            if not library.save_loaded_asset(mi,only_if_is_dirty=False):
                raise RuntimeError("真实Cliff MaterialInstance保存失败："+package)
            print("CLIFFROCK_UE_MI_SAVED",package)

    mat=library.load_asset(MATERIAL)
    if not isinstance(mat,unreal.Material):
        raise TypeError("Cliff预览母材质真正UE UMaterial回读失败")
    count=edit.get_num_material_expressions(mat)
    if count<10:
        raise RuntimeError("Cliff PBR材质节点不完整："+str(count))
    for package,parent_path,params in configs:
        inst=library.load_asset(package)
        if not isinstance(inst,unreal.MaterialInstanceConstant):
            raise TypeError("项目Cliff MI保存回读失败："+package)
        if inst.get_editor_property("parent")!=library.load_asset(parent_path):
            raise RuntimeError("Cliff MI继承母材质不正确："+package)
        for param,role in params:
            ptr=edit.get_material_instance_texture_parameter_value(inst,param)
            if ptr!=textures[role]:
                raise RuntimeError("Cliff PBR纹理参数不匹配："+package+":"+param)
    print("CLIFFROCK_UE_PBR_CANDIDATE_REAL_ASSETS_PASS",
          "textures=4 material=1 materialInstances=2 nodes",count,
          "oldRock01Unchanged=True manualReviewPending=True")

if __name__=="__main__":
    main()
