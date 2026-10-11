#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 岩石写实材质第01批：4K扫描源→真实Texture2D/PBR材质/项目地形MI。

所有资源归DivineBeasts第三层Village世界内容包，不改变平台Surface逻辑、
正式Village共享地图、现有10层技术校准母材质、已有MI和PCG图。
输入：RockRealism01_SourceManifest.json经Poly Haven官方MD5下载、SHA256逐张校验。
输出：4张Texture2D（BaseColor/NormalDX/ORM/Height），1个真正DefaultLit PBR
Material（基础色、法线、ORM的AO/粗糙度/金属度真实节点）、2个独立MI：
预览MI及继承项目现有10层母材质的Rock覆写MI，均标记人工审核待批准。
显示高度图只保存为后续HeightBlend/Nanite候选，不伪造WPO/Nanite Tessellation已实现。
ROCK_REALISM_UE_MODE=inspect只读；apply才允许创作，verify只允许实际UE对象回读；
所有同名已有资产拒绝覆盖/重建，避免破坏多人工作区。
"""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=(ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
        "DBAWorldPack_Village/SourceArt/Terrain/PBR/RockRealism01")
MANIFEST=SOURCE/"RockRealism01_SourceManifest.json"
TEXTURES="/DBAWorldPack_Village/Terrain/Realism/Rock01/Textures"
MATERIALS="/DBAWorldPack_Village/Terrain/Realism/Rock01/Materials"
PARENT="/DBAWorldPack_Village/Terrain/Materials/M_DBA_Terrain_MultiLayer10"
REVIEW=MATERIALS+"/M_DBA_RockRealism01_PBRReview"
REVIEW_MI=MATERIALS+"/MI_DBA_RockRealism01_PBRReview"
LANDSCAPE_MI=MATERIALS+"/MI_DBA_Terrain_RockRealism01"
MAPS={
    "BaseColor":TEXTURES+"/T_DBA_RockRealism01_BaseColor",
    "NormalDX":TEXTURES+"/T_DBA_RockRealism01_NormalDX",
    "ORM":TEXTURES+"/T_DBA_RockRealism01_ORM",
    "Height":TEXTURES+"/T_DBA_RockRealism01_Height",
}
MODE=os.getenv("ROCK_REALISM_UE_MODE","inspect").lower().strip()

def signed_sources()->dict[str,Path]:
    if not MANIFEST.is_file():
        raise FileNotFoundError("高精Rock候选扫描源清单未生成："+str(MANIFEST))
    manifest=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    if (manifest.get("schemaVersion")!=1 or
        manifest.get("kind")!="third_party_pbr_scan_candidate" or
        manifest.get("sourceId")!="rock_face_04" or
        manifest.get("sourceResolution")!="4k" or
        manifest.get("license")!="CC0 1.0" or
        manifest.get("productionAssetApproved") is not False):
        raise ValueError("岩石4K来源身份、开放许可或人工审核状态与合同不一致")
    matched={}
    for item in manifest.get("maps",[]):
        role=item["role"]
        if role in matched or role not in MAPS:
            raise ValueError("Rock源图未知或重复PBR通道："+role)
        file=(ROOT/item["localPath"]).resolve()
        if not file.is_relative_to(SOURCE.resolve()) or not file.is_file():
            raise FileNotFoundError("地形源图不属于Rock候选目录："+str(file))
        if not item["sourceUrl"].startswith(
            "https://dl.polyhaven.org/file/ph-assets/Textures/png/4k/rock_face_04/"):
            raise ValueError("未知第三方源素材URL："+role)
        if hashlib.sha256(file.read_bytes()).hexdigest()!=item["sha256"]:
            raise ValueError("扫描贴图SHA256校验失败："+str(file))
        matched[role]=file
    if set(matched)!=set(MAPS):
        raise ValueError("岩石PBR材质必须具备同源4图BaseColor/NormalDX/ORM/Height")
    return matched

def run()->None:
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("ROCK_REALISM_UE_MODE仅支持inspect/apply/verify")
    sources=signed_sources()
    print("ROCK_REALISM_IMPORT_PLAN",MODE,REVIEW,REVIEW_MI,LANDSCAPE_MI)
    if MODE=="inspect":
        return
    try:import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("只能由真实UE5.8 Editor创建Texture2D与材质资产") from exc
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前引擎未加载神兽联盟唯一主工程")
    assetlib=unreal.EditorAssetLibrary
    edit=unreal.MaterialEditingLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()

    # 拒绝在没有项目十层母材质时单独创建不匹配的项目MI。
    parent=assetlib.load_asset(PARENT)
    if not isinstance(parent,unreal.Material):
        raise TypeError("既有10层项目母材质不存在："+PARENT)
    textures={}
    for role,asset in MAPS.items():
        if assetlib.does_asset_exist(asset):
            obj=assetlib.load_asset(asset)
            if not isinstance(obj,unreal.Texture2D):
                raise TypeError("Rock候选包路径已被非Texture2D占用："+asset)
            textures[role]=obj
        elif MODE=="verify":
            raise FileNotFoundError("Rock真正UE Texture2D尚未创建："+asset)

    for asset in (REVIEW,REVIEW_MI,LANDSCAPE_MI):
        if assetlib.does_asset_exist(asset):
            obj=assetlib.load_asset(asset)
            intended=(unreal.Material if asset==REVIEW else unreal.MaterialInstanceConstant)
            if not isinstance(obj,intended):
                raise TypeError("已存在同名不同类别UObject，拒绝覆盖："+asset)
        elif MODE=="verify":
            raise FileNotFoundError("已保存Rock候选材质缺失："+asset)

    if MODE=="apply":
        pending=[]
        for role,source in sources.items():
            package=MAPS[role]
            if role in textures:
                print("ROCK_REALISM_EXISTING_TEXTURE_LEFT_UNCHANGED",package)
                continue
            folder,name=package.rsplit("/",1)
            if not assetlib.does_directory_exist(folder):
                assetlib.make_directory(folder)
            task=unreal.AssetImportTask()
            task.set_editor_property("filename",str(source))
            task.set_editor_property("destination_path",folder)
            task.set_editor_property("destination_name",name)
            task.set_editor_property("replace_existing",False)
            task.set_editor_property("automated",True)
            task.set_editor_property("save",False)
            pending.append((role,task))
        if pending:
            tools.import_asset_tasks([task for _,task in pending])
        compress={
            "BaseColor":unreal.TextureCompressionSettings.TC_DEFAULT,
            "NormalDX":unreal.TextureCompressionSettings.TC_NORMALMAP,
            "ORM":unreal.TextureCompressionSettings.TC_MASKS,
            "Height":unreal.TextureCompressionSettings.TC_GRAYSCALE,
        }
        for role,task in pending:
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE导入Rock扫描图失败："+role)
            obj=assetlib.load_asset(MAPS[role])
            if not isinstance(obj,unreal.Texture2D):
                raise TypeError("扫描源没有产生真正Texture2D："+MAPS[role])
            obj.set_editor_property("srgb",role=="BaseColor")
            obj.set_editor_property("compression_settings",compress[role])
            # 不能把DirectX法线重复反转绿通道；扫描nor_dx由来源直接输出。
            if role=="NormalDX":
                obj.set_editor_property("flip_green_channel",False)
            if not assetlib.save_loaded_asset(obj,only_if_is_dirty=False):
                raise RuntimeError("UE保存Rock扫描纹理失败："+role)
            print("ROCK_REALISM_TEXTURE_UE_SAVED",MAPS[role])
            textures[role]=assetlib.load_asset(MAPS[role])

    for role,asset in MAPS.items():
        obj=assetlib.load_asset(asset)
        if not isinstance(obj,unreal.Texture2D):
            raise RuntimeError("Rock实际Texture2D类型回读失败："+asset)
        textures[role]=obj

    if MODE=="apply" and not assetlib.does_asset_exist(REVIEW):
        folder,name=REVIEW.rsplit("/",1)
        if not assetlib.does_directory_exist(folder):
            assetlib.make_directory(folder)
        m=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(m,unreal.Material):
            raise RuntimeError("未能创建真实写实Rock材质UMaterial")

        def expr(cls,x,y):
            n=edit.create_material_expression(m,cls,int(x),int(y))
            if n is None:raise RuntimeError("UE材质表达式节点创建失败："+str(cls))
            return n

        def connect(a,b,pin,output=""):
            ok=edit.connect_material_expressions(a,output,b,pin)
            if not ok and pin=="Coordinates":
                ok=edit.connect_material_expressions(a,output,b,"UVs")
            if not ok:
                raise RuntimeError(f"Rock材质图连线失败 {a.get_name()} → {b.get_name()}:{pin}")

        uv=expr(unreal.MaterialExpressionTextureCoordinate,-1280,0)
        tiling=expr(unreal.MaterialExpressionScalarParameter,-1280,220)
        tiling.set_editor_property("parameter_name",unreal.Name("RockUVTiling"))
        tiling.set_editor_property("default_value",1.0)
        scaled=expr(unreal.MaterialExpressionMultiply,-1010,70)
        connect(uv,scaled,"A")
        connect(tiling,scaled,"B")

        def sample(role,param,x,y):
            node=expr(unreal.MaterialExpressionTextureSampleParameter2D,x,y)
            node.set_editor_property("parameter_name",unreal.Name(param))
            node.set_editor_property("texture",textures[role])
            if role=="NormalDX":
                node.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            elif role=="ORM":
                node.set_editor_property("sampler_type",unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            connect(scaled,node,"Coordinates")
            return node

        bc=sample("BaseColor","RockScanBaseColor",-720,-700)
        normal=sample("NormalDX","RockScanNormalDX",-720,-150)
        orm=sample("ORM","RockScanORM",-720,440)
        tint=expr(unreal.MaterialExpressionVectorParameter,-430,-850)
        tint.set_editor_property("parameter_name",unreal.Name("RockColorTint"))
        tint.set_editor_property("default_value",unreal.LinearColor(1,1,1,1))
        tintmul=expr(unreal.MaterialExpressionMultiply,-220,-670)
        connect(bc,tintmul,"A","RGB")
        connect(tint,tintmul,"B")
        if not edit.connect_material_property(tintmul,"",unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("Rock BaseColor PBR材质未连接")
        if not edit.connect_material_property(normal,"RGB",unreal.MaterialProperty.MP_NORMAL):
            raise RuntimeError("Rock Normal PBR材质未连接")

        # 真实扫描ARM输入通道按R=AO/G=Roughness/B=Metalness对应UE的物理输出。
        if not edit.connect_material_property(orm,"R",unreal.MaterialProperty.MP_AMBIENT_OCCLUSION):
            raise RuntimeError("Rock AO物理属性未连接")
        if not edit.connect_material_property(orm,"G",unreal.MaterialProperty.MP_ROUGHNESS):
            raise RuntimeError("Rock Roughness物理属性未连接")
        if not edit.connect_material_property(orm,"B",unreal.MaterialProperty.MP_METALLIC):
            raise RuntimeError("Rock Metallic物理属性未连接")
        count=edit.get_num_material_expressions(m)
        if count<8:
            raise RuntimeError("Rock全PBR材质图节点数不足")
        messages=[str(item) for item in edit.recompile_material(m)]
        for message in messages:
            print("ROCK_REALISM_MATERIAL_SHADER_DIAGNOSTIC",message)
        if any("error" in message.lower() for message in messages):
            raise RuntimeError("UE岩石真实材质Shader编译错误")
        if not assetlib.save_loaded_asset(m,only_if_is_dirty=False):
            raise RuntimeError("Rock写实预览母材质保存失败")
        print("ROCK_REALISM_PBR_MATERIAL_UE_SAVED",REVIEW,"nodes",count)

    if MODE=="apply" and not assetlib.does_asset_exist(REVIEW_MI):
        folder,name=REVIEW_MI.rsplit("/",1)
        mi=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("Rock预览MI未真实生成")
        edit.set_material_instance_parent(mi,assetlib.load_asset(REVIEW))
        for role,param in (("BaseColor","RockScanBaseColor"),("NormalDX","RockScanNormalDX"),("ORM","RockScanORM")):
            edit.set_material_instance_texture_parameter_value(mi,param,textures[role])
        edit.set_material_instance_scalar_parameter_value(mi,"RockUVTiling",1.0)
        if not assetlib.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("Rock预览MI保存失败")
        print("ROCK_REALISM_PBR_REVIEW_MI_UE_SAVED",REVIEW_MI)

    if MODE=="apply" and not assetlib.does_asset_exist(LANDSCAPE_MI):
        folder,name=LANDSCAPE_MI.rsplit("/",1)
        mi=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("Rock在10层Landscape上的项目MI未真实生成")
        edit.set_material_instance_parent(mi,parent)
        for role,param in (("BaseColor","TerrainRockBaseColor"),("NormalDX","TerrainRockNormal"),("ORM","TerrainRockORM")):
            edit.set_material_instance_texture_parameter_value(mi,param,textures[role])
        if not assetlib.save_loaded_asset(mi,only_if_is_dirty=False):
            raise RuntimeError("Rock对应十层Landscape MI保存失败")
        print("ROCK_REALISM_LANDSCAPE_MI_UE_SAVED",LANDSCAPE_MI)

    review=assetlib.load_asset(REVIEW)
    mat_instance=assetlib.load_asset(REVIEW_MI)
    land_instance=assetlib.load_asset(LANDSCAPE_MI)
    if (not isinstance(review,unreal.Material) or
        not isinstance(mat_instance,unreal.MaterialInstanceConstant) or
        not isinstance(land_instance,unreal.MaterialInstanceConstant)):
        raise TypeError("Rock资产保存后真实UE UObject类型回读失败")
    if (mat_instance.get_editor_property("parent")!=review or
        land_instance.get_editor_property("parent")!=parent):
        raise RuntimeError("Rock材质实例继承关系不正确")
    n=edit.get_num_material_expressions(review)
    if n<8:
        raise RuntimeError("Rock实际材质没有完整PBR表达式")
    for mi,links in (
        (mat_instance,(("RockScanBaseColor","BaseColor"),("RockScanNormalDX","NormalDX"),("RockScanORM","ORM"))),
        (land_instance,(("TerrainRockBaseColor","BaseColor"),("TerrainRockNormal","NormalDX"),("TerrainRockORM","ORM"))),
    ):
        for param,role in links:
            actual=edit.get_material_instance_texture_parameter_value(mi,param)
            if actual!=textures[role]:
                raise RuntimeError("Rock项目材质MI的实际扫描纹理映射不匹配："+param)
    print("ROCK_REALISM_UE_MATERIALS_VERIFIED textures=4 preview_material=1 review_MI=1 landscape_MI=1",
          "shader_nodes",n, "manual_approval_pending=True")


if __name__=="__main__":
    run()
