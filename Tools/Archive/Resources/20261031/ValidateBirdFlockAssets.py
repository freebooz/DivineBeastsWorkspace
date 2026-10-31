#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 鸟群首版历史资产只读回归校验；当前运行资产使用ValidateBirdFlockP4Assets.py。

必须在正式 UnrealEditor-Cmd PythonScriptPlugin 下运行。重新加载先前真实写盘的十件
AssetRegistry 资源（3 StaticMesh + 3 Texture2D + 3 MaterialInstanceConstant + 1 Material）。
只读，不覆盖资源，不创建临时假uasset；若缺项、错误类型、空父材质则抛异常。
该校验不等于蓝图/Niagara、实时飞行、Cook、GPU渲染或人工审美验收。
"""
from __future__ import annotations
raise RuntimeError('此首版鸟群校验器已退休归档；当前入口为Tools/Unreal/VFX/BirdFlock/ValidateBirdFlockP4Assets.py。')
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/BirdFlock"
BIRD="/DBAPresentationPack_Core/VFX/Environment/BirdFlock"
MASTER="/GamePlatformVFX/Environment/BirdFlock/Materials/M_GP_BirdFlock_Master"
SPECIES=("Seagull","Swallow","Eagle")


def main():
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("不是神兽联盟主工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("不是锁定 UE5.8")
    m=json.loads((SOURCE/"BirdFlockSourceManifest.json").read_text(encoding="utf-8"))
    assert m["schemaVersion"]==1
    for item in m["files"]:
        f=SOURCE/item["path"]
        if not f.is_file() or hashlib.sha256(f.read_bytes()).hexdigest()!=item["sha256"]:
            raise RuntimeError("源模型或羽纹资源哈希不匹配："+item["path"])
    lib=unreal.EditorAssetLibrary
    parent=lib.load_asset(MASTER)
    if not isinstance(parent,unreal.Material):
        raise RuntimeError("平台通用母材质丢失/无效")
    print("BIRD_VERIFY_PLATFORM_MASTER",parent.get_path_name())
    checked=1
    for name in SPECIES:
        mesh=lib.load_asset('/DBAPresentationPack_Core/Development/History/BirdFlock/Meshes/SM_DBA_Bird_'+name+'_Cm')
        tex=lib.load_asset(BIRD+"/Textures/T_DBA_Bird_"+name+"_Albedo")
        mat=lib.load_asset(BIRD+"/Materials/MI_DBA_Bird_"+name)
        if not isinstance(mesh,unreal.StaticMesh):
            raise TypeError("未找到真实UStaticMesh："+name)
        if not isinstance(tex,unreal.Texture2D):
            raise TypeError("未找到真实Texture2D："+name)
        if not isinstance(mat,unreal.MaterialInstanceConstant):
            raise TypeError("未找到真实材质实例："+name)
        mat_parent=mat.get_editor_property("parent")
        if mat_parent is None or mat_parent.get_path_name()!=parent.get_path_name():
            raise RuntimeError("材质实例没有正确继承平台母材质："+name)
        x,y=tex.blueprint_get_size_x(),tex.blueprint_get_size_y()
        if min(x,y)<256:
            raise RuntimeError("鸟群纹理分辨率异常："+name)
        dims=mesh.get_bounds().box_extent
        max_extent=max(abs(dims.x),abs(dims.y),abs(dims.z))
        # 三种真实鸟翼展开半宽必须在30..300cm量级，严防100倍误缩放。
        if max_extent<30.0 or max_extent>300.0:
            raise RuntimeError("鸟群FBX导入缩放错误："+name+"，半宽cm="+str(max_extent))
        print("BIRD_VERIFY_SPECIES_OK",name,
              "texture",x,y,
              "meshHalfExtentCm",round(dims.x,2),round(dims.y,2),round(dims.z,2))
        checked+=3
    print("BIRD_VERIFY_ALL_OK",checked,"source_manifest_files",len(m["files"]))
if __name__=="__main__":
    main()
