#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 客户端狼骨骼与动画蓝图软引用绑定作者。

用户通过Fab取得资源，完成纯内容包迁移后才允许执行apply。
不调用编译/烘焙/PIE，不调用完整角色AI能力；不自动开启不可见怪物。
需要提供真正AnimBlueprint和一致的Skeleton，拒绝空图或占位路径。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ASSET_MANIFEST = Path(__file__).with_name("LicensedAnimalVarietyImport.json")
INPUT = json.loads(ASSET_MANIFEST.read_text(encoding="utf-8-sig"))
MODE = os.environ.get("SSLM_WOLF_VISUAL_BIND_MODE", "inspect").strip().lower()


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def destination_path(original):
    source=INPUT["Source"]["FabImportedContentDirectory"].rstrip("/")
    destination=INPUT["Destination"]["ImportedAssetFolder"]
    require(source.startswith("/Game/") and original.startswith(source+"/"),
            "源资源路径必须属于已登记的Fab原始包")
    return destination + original[len(source):]


def format_object_path(path):
    """输入是UE对象路径或资产包路径，统一生成Config可识别的对象路径。"""
    if "." in path.rsplit("/", 1)[-1]:
        return path
    name=path.rsplit("/",1)[-1]
    return path+"."+name


def inspect():
    src=INPUT["Source"]
    fields=("FabImportedContentDirectory","WolfSkeletalMesh","WolfSkeleton",
            "WolfAnimationBlueprint")
    missing=[name for name in fields if not src.get(name)]
    print("VILLAGE_WOLF_CLIENT_BIND_INSPECT",json.dumps({
        "RequiredSourceFieldsMissing":missing,
        "LicenceSource":INPUT["Status"],
        "RuntimeBoolsChanged":False,"EditorAssetChanged":False,
        "BuildOrCook":False
    },ensure_ascii=False))


def apply():
    import unreal
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "只能在UE5.8真正编辑器执行")
    require(Path(unreal.Paths.get_project_file_path()).resolve() ==
            (ROOT/"Game/DivineBeastsArena.uproject").resolve(),
            "项目不匹配")
    require("-SSLMWolfVisualBind" in unreal.SystemLibrary.get_command_line(),
            "未经本次专用参数允许禁止修改配置")
    editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor is not None and editor.get_game_world() is None,
            "游戏运行中禁止修改配置")
    project=json.loads((ROOT/"Game/DivineBeastsArena.uproject").read_text(encoding="utf-8-sig"))
    plugin=INPUT["Destination"]["PluginName"]
    require(any(x.get("Name")==plugin and x.get("Enabled") is True for x in project["Plugins"]),
            "第三方动物资源未真实迁移和注册，拒绝写入软路径")

    src=INPUT["Source"]
    mesh_path=destination_path(src["WolfSkeletalMesh"])
    skeleton_path=destination_path(src["WolfSkeleton"])
    bp_path=destination_path(src["WolfAnimationBlueprint"])
    mesh=unreal.EditorAssetLibrary.load_asset(mesh_path)
    skeleton=unreal.EditorAssetLibrary.load_asset(skeleton_path)
    bp=unreal.EditorAssetLibrary.load_asset(bp_path)
    require(isinstance(mesh,unreal.SkeletalMesh) and isinstance(skeleton,unreal.Skeleton)
            and isinstance(bp,unreal.AnimBlueprint),
            "缺少来自正版狼资源的骨骼模型、骨架或动画蓝图")
    require(mesh.get_editor_property("skeleton")==skeleton,
            "狼模型和骨架身份不匹配")
    require(bp.get_editor_property("target_skeleton")==skeleton,
            "狼动画蓝图未绑定相同骨架")
    animation_class=unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
    require(animation_class is not None and isinstance(
        unreal.get_default_object(animation_class),unreal.AnimInstance),
        "狼动画蓝图不存在合法的AnimInstance生成类")
    # 这里仅审查真实类存在，不代替动画图及混合状态的人工逐帧验收。
    cfg_path=ROOT/"Game/Config/DefaultGame.ini"
    raw=cfg_path.read_text(encoding="utf-8-sig")
    require("bClientVisualsVerified=false" in raw and "bEnableVillageWolves=false" in raw,
            "当前安全开关不是关闭状态，不允许盲目更新")
    lines=raw.splitlines(keepends=True)
    new_lines=[]
    seen=set()
    for line in lines:
        if line.startswith("WolfSkeletalMesh="):
            require("Mesh" not in seen,"多份WolfSkeletalMesh配置冲突")
            seen.add("Mesh")
            line="WolfSkeletalMesh="+format_object_path(mesh_path)+"\n"
        if line.startswith("WolfAnimationBlueprint="):
            require("Anim" not in seen,"多份WolfAnimationBlueprint配置冲突")
            seen.add("Anim")
            line="WolfAnimationBlueprint="+str(animation_class.get_path_name())+"\n"
        new_lines.append(line)
    require(seen=={"Mesh","Anim"},"缺少狼外观软引用配置行")
    path_text="".join(new_lines)
    require(path_text!=raw,"已存在相同路径，没有必要重写配置")
    cfg_path.write_text(path_text,encoding="utf-8")
    print("VILLAGE_WOLF_CLIENT_BIND_PATHS_ONLY",json.dumps({
        "WolfSkeletalMesh":format_object_path(mesh_path),
        "WolfAnimationBlueprint":animation_class.get_path_name(),
        "Skeleton":format_object_path(skeleton_path),
        "bClientVisualsVerified":False,"bEnableVillageWolves":False,
        "Cooked":False,"AnimationGraphAcceptance":"Pending"
    },ensure_ascii=False))


def main():
    require(MODE in ("inspect","apply"),"只允许inspect或apply")
    if MODE=="inspect":
        inspect()
    else:
        apply()


if __name__=="__main__":
    main()
