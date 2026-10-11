#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟：复制独立CliffRock01审核地图，比较旧20m与共享扭曲20m。

原有L_DBA_CliffRock01Review审核图、正式L_Village_Start和ForestRock01不改。
只在新建的/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview
真实UE5.8 UWorld内更改两块20m测试板的材质，左新宏观WorldAligned，
右亮度平衡的UV基准。源地图需保存前后SHA256一致。
CLIFFROCK_MACRO_MAP_MODE=inspect/apply/verify，仅EditorCmd完成原生保存回读。
"""

from __future__ import annotations
import hashlib
import os
import gc
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE_MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01Review"
MAP="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
ORIGINAL_FILE=ROOT/"Game/Content/Development/Terrain/Maps/L_DBA_CliffRock01Review.umap"
NEW_FILE=ROOT/"Game/Content/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview.umap"
WORLD_MACRO="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro"
UV_BALANCED="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview"
MATERIALS={
    "CliffReview_WorldAligned_20m":WORLD_MACRO,
    "CliffReview_UV_20m":UV_BALANCED,
    "CliffReview_Ground":UV_BALANCED,
}
MODE=os.getenv("CLIFFROCK_MACRO_MAP_MODE","inspect").strip().lower()


def verify_world(unreal)->None:
    world=unreal.EditorLevelLibrary.get_editor_world()
    if not isinstance(world,unreal.World) or MAP not in world.get_path_name():
        raise TypeError("宏观Cliff审核地图不是已保存的实际UE5.8 UWorld")
    actors=unreal.EditorLevelLibrary.get_all_level_actors()
    bylabel={a.get_actor_label():a for a in actors}
    if len(bylabel)!=len(actors):
        raise RuntimeError("宏观审核Actor标签重复")
    for name,assetpath in MATERIALS.items():
        actor=bylabel.get(name)
        if not isinstance(actor,unreal.StaticMeshActor):
            raise TypeError("宏观审核场景缺少StaticMesh："+name)
        mi=unreal.EditorAssetLibrary.load_asset(assetpath)
        if not isinstance(mi,unreal.MaterialInstanceConstant):
            raise FileNotFoundError("Cliff候选MI缺失："+assetpath)
        comp=actor.get_editor_property("static_mesh_component")
        if comp.get_material(0)!=mi:
            raise RuntimeError("20米审核板未绑定预期材质："+name)
        if name!="CliffReview_Ground":
            sz=actor.get_actor_scale3d()
            if abs(sz.y-19.5)>.1 or abs(sz.z-13)>.1:
                raise RuntimeError("20米板的真实尺度被改变")
        print("CLIFF_MACRO_MAP_ACTOR_VERIFIED",name,assetpath)
    for label in ("CliffReview_Camera_20m","CliffReview_Sun",
                  "CliffReview_LeftSoftbox","CliffReview_RightSoftbox","CliffReview_Skylight"):
        if label not in bylabel:raise RuntimeError("场景光照或相机缺失："+label)
    print("CLIFF_MACRO_20M_REVIEW_MAP_UE_RELOAD_PASS",MAP,
          "worldMacroWall=1 uvBalancedWall=1 ground=1 lighting=original")


def main()->None:
    if MODE not in ("inspect","clone","configure","verify"):
        raise ValueError("CLIFFROCK_MACRO_MAP_MODE仅支持inspect/clone/configure/verify")
    print("CLIFF_MACRO_REVIEW_MAP_PLAN",MODE,SOURCE_MAP,MAP)
    if MODE=="inspect":return
    if not ORIGINAL_FILE.is_file():
        raise FileNotFoundError("已验证的原20米崖壁地图不存在")
    before=hashlib.sha256(ORIGINAL_FILE.read_bytes()).hexdigest()
    import unreal # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
        "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("没有加载正式神兽联盟工程")
    lib=unreal.EditorAssetLibrary
    levels=unreal.EditorLevelLibrary
    if not lib.does_asset_exist(SOURCE_MAP):
        raise FileNotFoundError("旧审核地图AssetRegistry未注册")
    for pkg in MATERIALS.values():
        if not isinstance(lib.load_asset(pkg),unreal.MaterialInstanceConstant):
            raise TypeError("项目材料或QA对照MI不可加载："+pkg)
    # 必须分为两个互相独立的UnrealEditor-Cmd进程执行！
    # 第一进程Clone完成后立刻退出；若同一Python回调内紧接着LoadLevel，
    # UWorld仍会被FPyReferenceCollector引用，UE5.8严格GC会Fatal。
    if MODE=="clone":
        if lib.does_asset_exist(MAP) or NEW_FILE.is_file():
            raise FileExistsError("Cliff宏观审核目标已存在，绝不覆盖")
        created=lib.duplicate_asset(SOURCE_MAP,MAP)
        if not isinstance(created,unreal.World):
            raise RuntimeError("UE AssetLibrary复制审核地图失败")
        saved=lib.save_loaded_asset(created,only_if_is_dirty=False)
        print("CLIFF_MACRO_CLONE_NATIVE_WORLD_SAVED",MAP,"saveResult",saved)
        del created
        gc.collect()
        # 在本进程绝不能再调用EditorLevelLibrary.load_level。
        if not NEW_FILE.is_file():
            raise RuntimeError("Cliff宏观新地图复制后仍没有真实UMAP文件")
    elif MODE in ("configure","verify"):
        if not lib.does_asset_exist(MAP) or not NEW_FILE.is_file():
            raise FileNotFoundError("Cliff审核克隆地图不存在，先在独立进程执行clone")
        if not levels.load_level(MAP):
            raise RuntimeError("UE加载克隆的CliffMacro UWorld失败")
        if MODE=="configure":
            actors={x.get_actor_label():x for x in levels.get_all_level_actors()}
            for label,pkg in MATERIALS.items():
                actor=actors.get(label)
                if not isinstance(actor,unreal.StaticMeshActor):
                    raise RuntimeError("Cliff宏观审核场景缺少测试网格："+label)
                actor.get_editor_property("static_mesh_component").set_material(0,lib.load_asset(pkg))
            if not levels.save_current_level():
                raise RuntimeError("Cliff宏观20米真实审核地图SavePackage失败")
            print("CLIFF_MACRO_REVIEW_WORLD_MATERIALS_SAVED",MAP)
        verify_world(unreal)
    if not NEW_FILE.is_file() or NEW_FILE.stat().st_size<20000:
        raise RuntimeError("新UE5.8独立审核地图没有实际保存为.umap")
    if hashlib.sha256(ORIGINAL_FILE.read_bytes()).hexdigest()!=before:
        raise RuntimeError("原20米测试图发生变化；禁止当前任务继续")
    print("CLIFF_MACRO_REVIEW_MAP_SOURCE_HASH_UNCHANGED",before)
    print("CLIFF_MACRO_REVIEW_MAP_NATIVE_UE_PASS sourceUntouched=True",MAP)


if __name__=="__main__":
    main()
