#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fab狼美术蓝图登记作者合同：只有用户拥有合法的网格、骨骼和真实AnimBP时才能保存引用。

不在当前无素材情况下制造假的动画蓝图，不启用 bClientVisualsVerified；
如源包仅提供26段动画而没有动画状态机，应先在UE动画蓝图编辑器人工装配完整状态机
Idle、Walk、Run、Attack、Hit、Death，再通过本文件审核其与真实Wolf Skeleton匹配。
"""
from __future__ import annotations

import json
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
SPEC_PATH = Path(__file__).with_name("VillageWolfAuthoringSpec.json")


def required_visual_assets() -> list[str]:
    source = json.loads(SPEC_PATH.read_text(encoding="utf-8-sig"))
    value = source["Visual"]
    return [value["SkeletalMeshAssetPath"], value["SkeletonAssetPath"],
            value["SourceAnimationBlueprintPath"]]


def inspect():
    paths = required_visual_assets()
    if not all(paths):
        print("SSLM_WOLF_VISUAL_BLOCKED: 尚未填入Fab真实狼骨骼、模型、动画蓝图路径，"
              "无法生成/声明可用外观。")
        return
    import unreal  # type: ignore
    library = unreal.EditorAssetLibrary
    mesh = library.load_asset(paths[0])
    skeleton = library.load_asset(paths[1])
    anim = library.load_asset(paths[2])
    if not isinstance(mesh, unreal.SkeletalMesh) or not isinstance(skeleton, unreal.Skeleton):
        raise RuntimeError("提供的狼模型或骨架非真实UE SkeletalMesh/Skeleton")
    if not isinstance(anim, unreal.AnimBlueprint):
        raise RuntimeError("必须提供真正的狼动画蓝图（包含完整状态机），不能以文本伪造")
    if mesh.get_editor_property("skeleton") != skeleton or anim.get_editor_property("target_skeleton") != skeleton:
        raise RuntimeError("狼Mesh、Skeleton和AnimBP骨架不一致")
    print("SSLM_WOLF_VISUAL_SOURCES_PRESENT_ONLY: 已找到三类UE资产，"
          "尚未进行动画状态机、商业许可、客户端Cook或运行验收。")


if __name__ == "__main__":
    inspect()
