#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 函数化十层地形独立审核地图原生作者。

仅将已有 /Game/Development/Terrain 的校准审核地图复制到新路径，
新地图的唯一 ALandscape 切换到函数化客户端审核MI；不影响正式Village地图、
既有地形权重、PCG Graph、地图原件和服务器权威。默认只读inspect。
"""
from __future__ import annotations

import os
from pathlib import Path

# copy 与 apply 必须在不同UE编辑器进程进行。复制UWorld仍存活时直接LoadLevel会触发EditorServer World Memory Leaks Fatal。
MODE = os.environ.get("TERRAIN_FUNCTION_REVIEW_MAP_MODE", "inspect").strip().lower()
SOURCE = "/Game/Development/Terrain/Maps/L_DBA_TerrainMaskReview"
TARGET = "/Game/Development/Terrain/Maps/L_DBA_TerrainFunctionReview"
REVIEW_MI = ("/DBAPresentationPack_Core/Materials/Terrain/"
             "MI_DBA_Terrain_Production10_FunctionIntegrated_Review")
ORIGINAL_MI = ("/DBAPresentationPack_Core/Materials/Terrain/"
               "MI_DBA_Terrain_Production10_Review")


def run() -> None:
    if MODE not in {"inspect", "copy", "apply", "verify"}:
        raise ValueError("审核地图作者只允许inspect/copy/apply/verify")
    print("TERRAIN_FUNCTION_REVIEW_MAP_PLAN", MODE, TARGET)
    if MODE == "inspect":
        return
    try:
        import unreal  # type: ignore
    except ImportError as exc:
        raise RuntimeError("审核地图必须由正式UE5.8编辑器生成") from exc
    if not str(unreal.Paths.get_project_file_path()).replace("\\", "/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在其他项目生成审核地图")

    lib = unreal.EditorAssetLibrary
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not lib.does_asset_exist(SOURCE):
        raise RuntimeError("源地形审核地图或真实关卡编辑器不存在")
    candidate = lib.load_asset(REVIEW_MI)
    if not isinstance(candidate, unreal.MaterialInstanceConstant):
        raise RuntimeError("函数化审核MI尚未真实生成")
    if candidate.get_editor_property("parent").get_path_name().split(".")[0] != (
            "/DBAPresentationPack_Core/Materials/Terrain/M_DBA_Terrain_Production10_FunctionIntegrated"):
        raise RuntimeError("审核MI未继承正确的函数化十层母材质")

    if MODE == "copy":
        if lib.does_asset_exist(TARGET):
            raise RuntimeError("审核地图目标已存在，不执行重复复制；应进入独立进程apply/verify")
        # 只复制并保存，不在本进程LoadLevel。UE5.8同时保留原图及新UWorld时会触发OldWorld泄漏Fatal。
        duplicate = lib.duplicate_asset(SOURCE, TARGET)
        if not isinstance(duplicate, unreal.World):
            raise RuntimeError("UE5.8未能复制原生地形UWorld")
        if not lib.save_asset(TARGET, only_if_is_dirty=False):
            raise RuntimeError("UE5.8未能保存新地图UWorld包")
        disk = (Path(__file__).resolve().parents[3] /
                "Game/Content/Development/Terrain/Maps/L_DBA_TerrainFunctionReview.umap")
        if not disk.is_file():
            raise RuntimeError("目标地图内存创建但磁盘没有UMAP，拒绝标记复制成功")
        print("TERRAIN_FUNCTION_REVIEW_MAP_COPY_SAVED", TARGET,
              "separate_editor_process_for_load_required=true")
        return

    if not lib.does_asset_exist(TARGET):
        raise RuntimeError("函数化审核地图尚未原生保存；必须先独立进程copy再进入apply/verify")

    if not editor.load_level(TARGET):
        raise RuntimeError("UE5.8无法打开函数化审核关卡")
    terrain = [actor for actor in unreal.EditorLevelLibrary.get_all_level_actors()
               if isinstance(actor, unreal.LandscapeProxy)]
    if len(terrain) != 1:
        raise RuntimeError("函数化审核关卡地形数量必须恰好为1：" + str(len(terrain)))
    current = terrain[0].get_editor_property("landscape_material")
    before = str(current.get_path_name()).split(".")[0] if current else ""
    if MODE == "apply":
        # 重试幂等：上次成功保存时不重复写入，发现第三方材质更改则拒绝覆盖。
        if before == ORIGINAL_MI:
            terrain[0].set_editor_property("landscape_material", candidate)
            if not editor.save_current_level():
                raise RuntimeError("UE5.8未能保存函数化独立地形审核关卡")
        elif before != REVIEW_MI:
            raise RuntimeError("审核地图已经绑定其他材质，拒绝覆盖：" + before)
    after = terrain[0].get_editor_property("landscape_material")
    actual = str(after.get_path_name()).split(".")[0] if after else ""
    if actual != REVIEW_MI:
        raise RuntimeError("审核关卡实际Landscape材质未绑定候选MI：" + actual)
    if not bool(terrain[0].get_editor_property("use_dynamic_material_instance")):
        raise RuntimeError("原始Landscape的动态材质实例开关意外丢失")
    print("TERRAIN_FUNCTION_REVIEW_MAP_UE_VERIFIED", TARGET,
          "landscape=1", "material="+actual,
          "original_map_unchanged=true")


if __name__ == "__main__":
    run()
