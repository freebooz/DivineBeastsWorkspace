#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CliffRock01按材质创建独立D3D12审核UWorld，杜绝运行时换材质导致截屏滞后一帧。

每个候选从同一批准的MacroReview地图复制并配置；clone/configure/verify均单独UE进程。
源地图只读，SHA256前后必须完全一致；目标路径只允许开发Terrain/Maps。
不覆盖已有人工uasset，碰到类型不符或材质已手改直接失败关闭。
"""
from __future__ import annotations

import gc
import hashlib
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE="/Game/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview"
SOURCE_FILE=ROOT/"Game/Content/Development/Terrain/Maps/L_DBA_CliffRock01MacroReview.umap"
VARIANTS={
    "baseline":(
        "/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpBaselineReview",
        "/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAligned",
    ),
    "lowWarp":(
        "/Game/Development/Terrain/Maps/L_DBA_CliffRock01LowWarpVariantReview",
        "/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_WorldAlignedMacro_LowWarpReview",
    ),
}
ORIGINAL="/DBAWorldPack_Village/Terrain/Realism/CliffRock01/Materials/MI_DBA_CliffRock01_WorldAlignedMacro"
UV="/Game/Development/Terrain/Materials/MI_DBA_CliffRock01_UVBalancedReview"
MODE=os.getenv("CLIFF_ISOLATED_MAP_MODE","inspect").lower().strip()
TYPE=os.getenv("CLIFF_ISOLATED_MAP_TYPE","baseline").strip()

def main():
    if MODE not in ("inspect","clone","configure","verify") or TYPE not in VARIANTS:
        raise ValueError("仅允许预审/克隆/配置/回读，且baseline或lowWarp两种")
    target,mi_path=VARIANTS[TYPE]
    print("CLIFF_LOW_ISOLATED_MAP_PLAN",MODE,TYPE,SOURCE,target,mi_path)
    if MODE=="inspect":return
    if not SOURCE_FILE.is_file():
        raise FileNotFoundError("原MacroReview UE5.8审核地图不存在")
    original_sha=hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest()
    try:import unreal
    except ImportError as ex:raise RuntimeError("必须由真正UE5.8Editor生成.umap") from ex
    if not unreal.Paths.get_project_file_path().replace("\\","/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("不能从其它工程制作Cliff测试地图")
    lib=unreal.EditorAssetLibrary
    for path in (SOURCE,ORIGINAL,UV,mi_path):
        if not lib.does_asset_exist(path):
            raise FileNotFoundError("缺少已批准源地图或材质："+path)
    for path in (ORIGINAL,UV,mi_path):
        if not isinstance(lib.load_asset(path),unreal.MaterialInstanceConstant):
            raise TypeError("审核材质不是合法UE MaterialInstance："+path)
    target_file=ROOT/"Game/Content/Development/Terrain/Maps"/(target.rsplit("/",1)[1]+".umap")
    if MODE=="clone":
        if target_file.exists() or lib.does_asset_exist(target):
            raise FileExistsError("独立Cliff QA地图已经存在，不允许覆盖")
        obj=lib.duplicate_asset(SOURCE,target)
        if not isinstance(obj,unreal.World):
            raise RuntimeError("原生UE复制出的关卡不是UWorld")
        if not lib.save_loaded_asset(obj,only_if_is_dirty=False):
            raise RuntimeError("真实QA关卡Package保存失败")
        del obj
        gc.collect()
        if not target_file.exists() or target_file.stat().st_size<18000:
            raise RuntimeError("实际QA的.umap没有写出")
        print("CLIFF_LOW_ISOLATED_WORLD_CLONE_SAVED",TYPE,target)
    else:
        if not target_file.is_file() or not lib.does_asset_exist(target):
            raise FileNotFoundError("请先在单独进程克隆QA场景："+target)
        if not unreal.EditorLevelLibrary.load_level(target):
            raise RuntimeError("UE没能真实打开独立Cliff QA UWorld")
        world=unreal.EditorLevelLibrary.get_editor_world()
        if not isinstance(world,unreal.World) or target not in world.get_path_name():
            raise RuntimeError("当前不是当前候选QA UWorld")
        actors={a.get_actor_label():a for a in unreal.EditorLevelLibrary.get_all_level_actors()}
        lhs=actors.get("CliffReview_WorldAligned_20m")
        rhs=actors.get("CliffReview_UV_20m")
        if not isinstance(lhs,unreal.StaticMeshActor) or not isinstance(rhs,unreal.StaticMeshActor):
            raise RuntimeError("20米岩壁静态网格不完整")
        left=lhs.get_editor_property("static_mesh_component")
        right=rhs.get_editor_property("static_mesh_component")
        original=lib.load_asset(ORIGINAL)
        required=lib.load_asset(mi_path)
        control=lib.load_asset(UV)
        if right.get_material(0)!=control:
            raise RuntimeError("原始独立QA地图对照UV已经改变")
        if MODE=="configure":
            if left.get_material(0) not in (original,required):
                raise RuntimeError("该独立QA地图20m材质已被外部人工修改")
            left.set_material(0,required)
            if left.get_material(0)!=required:
                raise RuntimeError("20米板材UE组件材质写入失败")
            if not unreal.EditorLevelLibrary.save_current_level():
                raise RuntimeError("独立QA地图保存失败")
            print("CLIFF_LOW_ISOLATED_WORLD_MATERIAL_SAVED",TYPE,mi_path)
        if left.get_material(0)!=required:
            raise RuntimeError("独立QA地图持久化材质不是本候选："+mi_path)
        for label in ("CliffReview_Camera_20m","CliffReview_Sun",
                      "CliffReview_LeftSoftbox","CliffReview_RightSoftbox",
                      "CliffReview_Skylight"):
            if label not in actors:
                raise RuntimeError("摄影机和固定照明组件不完整："+label)
        print("CLIFF_LOW_ISOLATED_WORLD_REAL_UE_VERIFY_PASS",TYPE,target,
              "20mWorldWall=1 uvControl=1 lights=4")
    if hashlib.sha256(SOURCE_FILE.read_bytes()).hexdigest()!=original_sha:
        raise RuntimeError("未经授权修改了原MacroReview地图，禁止继续")
    print("CLIFF_LOW_ISOLATED_BASE_MAP_SHA_UNCHANGED",original_sha)

if __name__=="__main__":
    main()
