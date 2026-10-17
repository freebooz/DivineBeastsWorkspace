#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在真正的UE5.8中用合法Fab狼骨架新建空的骨架绑定动画蓝图底稿。

本工具只创建具备真实Skeleton身份的原生AnimBlueprint壳。
不自动臆造AnimGraph节点、状态机或过渡逻辑，也不调用蓝图编译/PIE/Cook；
完整Idle/Walk/Run/Attack/Hit/Death动画图仍须在编辑器或已验证的Monolith工具中构建。

仅当已授权模型与狼动画序列全部迁入DBAContentPack_AnimalsProtofactor后可运行。
当源Fab已有动画蓝图时优先复用原图，不重复新建。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
M=Path(__file__).with_name("LicensedAnimalVarietyImport.json")
S=json.loads(M.read_text(encoding="utf-8-sig"))
SPEC=json.loads(Path(__file__).with_name("VillageWolfAuthoringSpec.json").read_text(encoding="utf-8-sig"))
MODE=os.environ.get("SSLM_WOLF_ANIM_AUTHOR_MODE","inspect").strip().lower()
OWNER_TAG="SSLMWolfAnimAuthor"
OWNER_VALUE="SSLM.NativeAnimBlueprintSkeletonOnly.v1"


def require(ok, text):
    if not ok:
        raise RuntimeError(text)


def destination(source: str) -> str:
    src=S["Source"]["FabImportedContentDirectory"].rstrip("/")
    dst=S["Destination"]["ImportedAssetFolder"]
    require(src.startswith("/Game/") and source.startswith(src+"/"),
            "狼资源必须来自已授权的Fab独立源目录")
    return dst+source[len(src):]


def preflight_editor(unreal):
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "只支持真实UE5.8编辑器")
    require(Path(unreal.Paths.get_project_file_path()).resolve() ==
            (ROOT/"Game/DivineBeastsArena.uproject").resolve(),
            "拒绝在其他工程修改")
    require("-SSLMWolfAnimSkeletonAuthoring" in unreal.SystemLibrary.get_command_line(),
            "缺少受控动画蓝图作者标志")
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor is not None and editor.get_game_world() is None,
            "游戏运行中不能创建动画蓝图")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() and
            not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),
            "存在未保存资产/地图，不可覆盖其它会话")
    project=json.loads((ROOT/"Game/DivineBeastsArena.uproject").read_text(encoding="utf-8-sig"))
    name=S["Destination"]["PluginName"]
    require(any(x.get("Name")==name and x.get("Enabled") is True
                for x in project.get("Plugins",[])),
            "动物纯内容插件尚未登记，拒绝空动画资产")
    source=S["Source"]
    require(bool(source["WolfSkeletalMesh"]) and bool(source["WolfSkeleton"]),
            "缺少合法狼骨骼资源对象路径")
    mesh_path=destination(source["WolfSkeletalMesh"])
    skeleton_path=destination(source["WolfSkeleton"])
    lib=unreal.EditorAssetLibrary
    mesh=lib.load_asset(mesh_path)
    skeleton=lib.load_asset(skeleton_path)
    require(isinstance(mesh,unreal.SkeletalMesh) and isinstance(skeleton,unreal.Skeleton),
            "Fab原生骨骼/网格迁移尚未完成")
    require(mesh.get_editor_property("skeleton")==skeleton,
            "狼网格与骨骼不匹配")
    for action,source_path in source["WolfAnimationSequences"].items():
        require(bool(source_path),"缺少关键狼动画对象路径："+action)
        sequence=lib.load_asset(destination(source_path))
        require(isinstance(sequence,unreal.AnimSequence),
                "找不到狼动画序列："+action)
        require(sequence.get_editor_property("skeleton")==skeleton,
                "狼动画序列使用不同骨架："+action)
    return lib, mesh, skeleton


def run():
    require(MODE in ("inspect","apply"),
            "仅允许inspect或apply，不接受自动图编译")
    target=SPEC["Visual"]["AnimationBlueprintTargetPath"]
    require(target=="/DBAContentPack_AnimalsProtofactor/Wolf/Animation/ABP_DBA_Wolf",
            "动画蓝图目标挂载点未在受控白名单内")
    if MODE=="inspect":
        print("SSLM_WOLF_ANIM_TARGET",json.dumps({
            "BlueprintToCreateWhenAssetsReady":target,
            "SourceFabAnimBlueprintPresent":bool(S["Source"]["WolfAnimationBlueprint"]),
            "Stage":"SkeletonOnly_NoAnimationGraph",
            "ActualAssetCreated":False,
            "RunsCompilation":False
        },ensure_ascii=False))
        return
    import unreal  # type: ignore
    lib,mesh,skeleton=preflight_editor(unreal)
    require(not S["Source"]["WolfAnimationBlueprint"],
            "源Fab已经提供动画蓝图，请直接复用并审查完整状态机，不要创建重复底稿")
    existing=lib.load_asset(target) if lib.does_asset_exist(target) else None
    if existing:
        require(isinstance(existing,unreal.AnimBlueprint),
                "目标已有其他类型资产，禁止覆盖")
        require(lib.get_metadata_tag(existing,OWNER_TAG)==OWNER_VALUE,
                "已有动画蓝图不是本作者生成，拒绝更改")
        require(existing.get_editor_property("target_skeleton")==skeleton,
                "既有动画蓝图与真正Fab狼骨架不一致")
        print("SSLM_WOLF_ANIM_SKELETON_BLUEPRINT_EXISTS",target)
        return
    factory=unreal.AnimBlueprintFactory()
    factory.set_editor_property("target_skeleton",skeleton)
    factory.set_editor_property("preview_skeletal_mesh",mesh)
    factory.set_editor_property("parent_class",unreal.AnimInstance)
    factory.set_editor_property("edit_after_new",False)
    folder,name=target.rsplit("/",1)
    require(lib.make_directory(folder) or lib.does_directory_exist(folder),
            "UE无法创建独立动物包的狼动画目录")
    bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,folder,unreal.AnimBlueprint,factory)
    require(isinstance(bp,unreal.AnimBlueprint),
            "未能创建原生Unreal AnimBlueprint")
    require(bp.get_editor_property("target_skeleton")==skeleton,
            "生成动画蓝图所用真实Skeleton不符")
    lib.set_metadata_tag(bp,OWNER_TAG,OWNER_VALUE)
    require(lib.save_loaded_asset(bp,only_if_is_dirty=False),
            "UE未保存动画蓝图骨架底稿")
    report=ROOT/"Saved/Validation/VillageWolf/Authoring/AnimBlueprintSkeletonOnly.json"
    report.parent.mkdir(parents=True,exist_ok=True)
    report.write_text(json.dumps({
        "Project":"SSLM",
        "AnimationBlueprint":target,
        "Skeleton":skeleton.get_path_name(),
        "SkeletalMesh":mesh.get_path_name(),
        "Stage":"SkeletonOnly_NoAnimGraph_DoNotEnableRuntime",
        "Compiled":False,"Cooked":False,"AnimationTransitionsAuthored":False
    },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_WOLF_ANIM_NATIVE_SKELETON_ONLY_SAVED",target)


if __name__=="__main__":
    run()
