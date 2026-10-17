#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 已授权狼美术资源检查入口（只读）。

统一使用 LicensedAnimalVarietyImport.json 作为Fab原始资源唯一清单，
避免出现一份文件说动画已导入、另一份作者清单仍为空的双重事实。
检查模型、Skeleton、PhysicsAsset及六类动画是否兼容；对于Fab未提供AnimBP的情况，
仅报告需要在迁移后基于真实骨架制作，不能声称狼动画系统已完成。
不修改资产、不启用服务器刷怪，也不执行编译或测试。
"""
from __future__ import annotations

import json
from pathlib import Path

WORKSPACE=Path(__file__).resolve().parents[3]
CONFIG=Path(__file__).with_name("LicensedAnimalVarietyImport.json")
AUTHOR=Path(__file__).with_name("VillageWolfAuthoringSpec.json")


def required_visual_assets() -> dict:
    """返回具名目标，而不是位置列表，防止WolfMesh/Skeleton字段位置错配。"""
    data=json.loads(CONFIG.read_text(encoding="utf-8-sig"))
    return data["Source"]


def inspect():
    source=required_visual_assets()
    names=["FabImportedContentDirectory","WolfSkeletalMesh","WolfSkeleton"]
    missing=[name for name in names if not source.get(name)]
    missing.extend("WolfAnimationSequences."+name for name,value
                   in source["WolfAnimationSequences"].items() if not value)
    if missing:
        print("SSLM_WOLF_VISUAL_INPUTS_MISSING",json.dumps(missing,ensure_ascii=False))
        return

    import unreal  # type: ignore
    library=unreal.EditorAssetLibrary
    stage=json.loads(CONFIG.read_text(encoding="utf-8-sig"))
    base=source["FabImportedContentDirectory"].rstrip("/")
    target=stage["Destination"]["ImportedAssetFolder"]
    migrated=library.does_directory_exist(target)
    def real(name):
        path=source[name]
        if migrated and path.startswith(base+"/"):
            path=target+path[len(base):]
        return library.load_asset(path)

    mesh=real("WolfSkeletalMesh")
    skeleton=real("WolfSkeleton")
    physics=real("WolfPhysicsAsset") if source.get("WolfPhysicsAsset") else None
    if not isinstance(mesh,unreal.SkeletalMesh) or not isinstance(skeleton,unreal.Skeleton):
        raise RuntimeError("必须通过真实UE5.8编辑器导入狼SkeletalMesh和Skeleton")
    if physics is not None and not isinstance(physics,unreal.PhysicsAsset):
        raise RuntimeError("狼PhysicsAsset路径存在，但不是原生物理资产")
    if mesh.get_editor_property("skeleton")!=skeleton:
        raise RuntimeError("狼网格和骨架不匹配")
    for name,path in source["WolfAnimationSequences"].items():
        resolved=target+path[len(base):] if migrated and path.startswith(base+"/") else path
        animation=library.load_asset(resolved)
        if not isinstance(animation,unreal.AnimSequence):
            raise RuntimeError("没有真实狼动画序列："+name)
        if animation.get_editor_property("skeleton")!=skeleton:
            raise RuntimeError("狼动画序列的骨骼不兼容："+name)

    bp_source=source.get("WolfAnimationBlueprint","")
    if bp_source:
        bp_path=target+bp_source[len(base):] if migrated and bp_source.startswith(base+"/") else bp_source
    else:
        bp_path=json.loads(AUTHOR.read_text(encoding="utf-8-sig"))["Visual"]["AnimationBlueprintTargetPath"]
    blueprint=library.load_asset(bp_path) if library.does_asset_exist(bp_path) else None
    if blueprint is not None:
        if not isinstance(blueprint,unreal.AnimBlueprint):
            raise RuntimeError("动画蓝图路径被其他资产占用："+bp_path)
        if blueprint.get_editor_property("target_skeleton")!=skeleton:
            raise RuntimeError("动画蓝图目标骨架与狼Skeleton不一致")

    print("SSLM_WOLF_VISUAL_READONLY_RESULT",json.dumps({
        "OriginalFabAssetDirectory":base,
        "AssetsMigratedToDedicatedContentPack":migrated,
        "WolfSkeleton":skeleton.get_path_name(),
        "SixRequiredAnimationSequencesPresent":True,
        "OptionalPhysicsAssetPresent":isinstance(physics,unreal.PhysicsAsset),
        "AnimationBlueprintPresent":blueprint is not None,
        "AnimationBlueprintGraphAccepted":False,
        "BuildCookOrRuntimeValidated":False,
        "ServerSpawnSwitchChanged":False
    },ensure_ascii=False))


if __name__=="__main__":
    inspect()

if __name__ == "__main__":
    inspect()
