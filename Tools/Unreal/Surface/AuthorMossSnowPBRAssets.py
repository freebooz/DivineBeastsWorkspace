#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8平台通用苔藓/积雪PBR资产创作。

只允许在真实编辑器中导入Texture2D、生成材质和项目实例。
不修改既有雪母材质、正式Village共享地图或服务器天气权威。
MOSS_SURFACE_MODE默认inspect，apply显式操作，verify只读资产合同。
"""
import hashlib
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
MANIFEST=Path(__file__).with_name("MossPBRSourceManifest.json")
MODE=os.getenv("MOSS_SURFACE_MODE","inspect").strip().lower()
PLATFORM="/GamePlatformSurface"
MOSS=PLATFORM+"/Textures/Moss"
MASTER=PLATFORM+"/Materials/M_GP_Surface_SnowMoss_PBR"
INSTANCE="/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff"
MPC=PLATFORM+"/ParameterCollections/MPC_GP_SurfaceGlobal"

def run():
    if MODE not in ("inspect","verify","apply"):
        raise ValueError("MOSS_SURFACE_MODE只允许inspect/verify/apply")
    manifest=json.loads(MANIFEST.read_text(encoding="utf8"))
    if manifest["schemaVersion"]!=1 or len(manifest["maps"])!=5:
        raise ValueError("苔藓源图合同非法")
    for rec in manifest["maps"]:
        src=(ROOT/rec["path"]).resolve()
        if not src.is_relative_to(ROOT) or hashlib.sha256(src.read_bytes()).hexdigest()!=rec["sha256"]:
            raise ValueError("苔藓源图不可信或已变更："+str(src))

    targets={r["role"]:MOSS+"/"+Path(r["path"]).stem.removesuffix("_Source")
             for r in manifest["maps"]}
    for name in (MASTER,INSTANCE,*targets.values()):
        print("MOSS_PBR_ASSET_TARGET",name)
    if MODE=="inspect": return
    import unreal  # 只在真正的虚幻编辑器中导入资产
    project=unreal.Paths.get_project_file_path().replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前不是唯一正式神兽联盟工程："+project)
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    edit=unreal.MaterialEditingLibrary

    if MODE=="apply":
        for record in manifest["maps"]:
            target=targets[record["role"]]
            if assets.does_asset_exist(target): continue
            if not assets.does_directory_exist(MOSS): assets.make_directory(MOSS)
            task=unreal.AssetImportTask()
            task.set_editor_property("filename",str(ROOT/record["path"]))
            task.set_editor_property("destination_path",MOSS)
            task.set_editor_property("destination_name",target.rsplit("/",1)[-1])
            task.set_editor_property("automated",True)
            task.set_editor_property("replace_existing",False)
            task.set_editor_property("save",False)
            tools.import_asset_tasks([task])
            if not task.get_editor_property("imported_object_paths"):
                raise RuntimeError("UE Texture2D导入失败："+target)

            tex=assets.load_asset(target)
            if not isinstance(tex,unreal.Texture2D): raise TypeError("纹理类别错误："+target)
            tex.set_editor_property("srgb",record["role"]=="albedo")
            modes={"normal":unreal.TextureCompressionSettings.TC_NORMALMAP,
                   "orm":unreal.TextureCompressionSettings.TC_MASKS,
                   "coverage":unreal.TextureCompressionSettings.TC_MASKS,
                   "height":unreal.TextureCompressionSettings.TC_MASKS}
            tex.set_editor_property("compression_settings",modes.get(record["role"],unreal.TextureCompressionSettings.TC_DEFAULT))
            if not assets.save_loaded_asset(tex,only_if_is_dirty=False):
                raise RuntimeError("纹理保存失败："+target)

    moss={role:assets.load_asset(target) for role,target in targets.items()}
    if not all(isinstance(t,unreal.Texture2D) for t in moss.values()):
        raise TypeError("缺少真实苔藓Texture2D")
    snow_paths={"albedo":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowAlbedo",
                "normal":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowNormal",
                "orm":PLATFORM+"/Textures/Weather/T_GP_Weather_SnowORM",
                "coverage":PLATFORM+"/Textures/Snow/T_GP_Snow_CoverageNoise"}
    snow={k:assets.load_asset(v) for k,v in snow_paths.items()}
    mpc=assets.load_asset(MPC)
    if not all(isinstance(t,unreal.Texture2D) for t in snow.values()) or not isinstance(mpc,unreal.MaterialParameterCollection):
        raise TypeError("缺少平台已交付的雪贴图或Surface MPC")
    if MODE=="verify": return validate(assets, edit, unreal)

    if not assets.does_asset_exist(MASTER):
        folder,name=MASTER.rsplit("/",1)
        if not assets.does_directory_exist(folder): assets.make_directory(folder)
        mat=tools.create_asset(name,folder,unreal.Material,unreal.MaterialFactoryNew())
        if not isinstance(mat,unreal.Material): raise RuntimeError("无法创建真实苔藓积雪母材质")
    else:
        mat=assets.load_asset(MASTER)
        if not isinstance(mat,unreal.Material): raise TypeError("材质同名身份不正确")
    if edit.get_num_material_expressions(mat)==0:
        build_graph(unreal, edit, mat, mpc, moss, snow)

        diagnostics=list(edit.recompile_material(mat))
        if any("error" in str(e).lower() for e in diagnostics):
            raise RuntimeError("原生Shader编译失败："+str(diagnostics))
        if not assets.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("UE母材质未能保存")
    if not assets.does_asset_exist(INSTANCE):
        folder,name=INSTANCE.rsplit("/",1)
        if not assets.does_directory_exist(folder): assets.make_directory(folder)
        mi=tools.create_asset(name,folder,unreal.MaterialInstanceConstant,
                              unreal.MaterialInstanceConstantFactoryNew())
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise RuntimeError("UE未能创建项目苔藓实例")
