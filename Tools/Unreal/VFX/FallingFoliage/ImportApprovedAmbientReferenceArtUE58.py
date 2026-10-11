# -*- coding: utf-8 -*-
"""将用户高品质RGBA参考图集所提取的独立精灵正式导入神兽联盟UE5.8。

本脚本只处理用户手动提供到DBAPresentationPack_Core/SourceArt/AmbientNature/ExternalReferences
中的真实参考文件，并要求与SourceManifest.json SHA-256逐字节一致。
默认INSPECT，不覆盖任何已有Texture。APPROVED_APPLY才为五类图集创建新增纹理，
备份并更新现有项目材质实例LeafAlbedo参数；不修改平台母材质或Niagara运动代码。
"""
from __future__ import annotations
import json
import os
import hashlib
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
SRC=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature/ExternalReferences"
MODE=os.environ.get("AMBIENT_REFERENCE_MODE","inspect").strip().lower()
OUT=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/ReferenceArtImport.json"
BINARY=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment"
DEST="/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Textures"
SPECIES=("PearPetal","CherryPetal","PlaneLeaf","Ginkgo","Maple")

def select_art():
    path=SRC/"SourceManifest.json"
    if not path.is_file():
        raise FileNotFoundError("真实高品质图集没有传入工作区；请将下载的ZIP解压到ExternalReferences目录："+str(path))
    manifest=json.loads(path.read_text(encoding="utf-8"))
    selected=[]
    for style in SPECIES:
        group=next((g for g in manifest["groups"] if g["styleId"]==style),None)
        if not group or not group["cropAssets"]:
            raise RuntimeError("原图清单缺少风格："+style)
        first=group["cropAssets"][0]
        source=SRC/first["file"]
        if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest()!=first["sha256"]:
            raise RuntimeError("原始精灵SHA-256验证失败："+str(source))
        selected.append((style,source))
    return selected

def main():
    items=select_art()  # 即使inspect也必须拒绝假文件。
    print("AMBIENT_REFERENCE_SOURCE_VERIFIED",len(items))
    if MODE=="inspect":
        return
    if MODE!="approved_apply":
        raise ValueError("模式只能inspect或approved_apply")
    import unreal # 仅UE Editor内部可以运行，不允许普通Python伪造.uasset。
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject") or not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("正式资源修改必须在神兽联盟UE5.8编辑器中进行")
    assets=unreal.EditorAssetLibrary
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    library=unreal.MaterialEditingLibrary
    for style,_ in items:
        target=DEST+"/T_DBA_AmbientReference_"+style+"_V01"
        if assets.does_asset_exist(target):
            raise FileExistsError("高品质参考贴图已有正式资源，禁止覆盖："+target)
    for style,source in items:
        target_name="T_DBA_AmbientReference_"+style+"_V01"
        task=unreal.AssetImportTask()
        task.set_editor_property("filename",str(source))
        task.set_editor_property("destination_path",DEST)
        task.set_editor_property("destination_name",target_name)
        task.set_editor_property("automated",True)
        task.set_editor_property("replace_existing",False)
        task.set_editor_property("save",False)
        tools.import_asset_tasks([task])
        tex=assets.load_asset(DEST+"/"+target_name)
        if not isinstance(tex,unreal.Texture2D):
            raise RuntimeError("真实纹理导入失败："+target_name)
        tex.set_editor_property("srgb",True)
        tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_DEFAULT)
        if not assets.save_loaded_asset(tex,only_if_is_dirty=False):
            raise RuntimeError("高品质贴图保存失败："+target_name)
        if style in ("Ginkgo","Maple"):
            material_path="/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Materials/MI_DBA_Foliage_"+style
            disk=BINARY/"FallingFoliage/Materials"/("MI_DBA_Foliage_"+style+".uasset")
        else:
            material_path="/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Materials/MI_DBA_Ambient_"+style
            disk=BINARY/"AmbientNature/Materials"/("MI_DBA_Ambient_"+style+".uasset")
        material=assets.load_asset(material_path)
        if not isinstance(material,unreal.MaterialInstanceConstant):
            raise RuntimeError("高品质贴图找不到项目材质实例："+material_path)
        backup=ROOT/"Saved/Validation/FallingFoliage/ExpandedLibrary/ReferenceArtBackup"/disk.relative_to(BINARY)
        backup.parent.mkdir(parents=True,exist_ok=True)
        if not backup.exists():
            shutil.copy2(disk,backup)
        library.set_material_instance_texture_parameter_value(material,unreal.Name("LeafAlbedo"),tex)
        if not assets.save_loaded_asset(material,only_if_is_dirty=False):
            raise RuntimeError("参考材质实例保存失败："+material_path)
        print("AMBIENT_REFERENCE_UE_IMPORTED",style,source,material_path,flush=True)
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps({"success":True,"styles":[x[0] for x in items],
        "source":"UserImageGenOriginalCrops","mode":MODE},ensure_ascii=False,indent=2)+"\n",encoding="utf8")

if __name__=="__main__":
    main()
