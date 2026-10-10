# -*- coding: utf-8 -*-
"""PCG金标准开发资产链静态合同回归（不依赖UEEditor，只校验创作一致性）。

本测试不生成资源、不运行UE、不修改源码；后续必须独立运行UE自动化和地图G01～G16。
"""
from __future__ import annotations

import ast
import re
import runpy
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[2]
TOOLS = WORKSPACE / "Tools/Unreal/PCG"
PCG = WORKSPACE / "Game/Plugins/GamePlatform/World/GamePlatformPCG"
GAME_CONFIG = WORKSPACE / "Game/Config/DefaultGame.ini"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def assert_equal(actual, expected, name: str) -> None:
    if actual != expected:
        raise AssertionError(f"{name}：实际={actual!r}，期望={expected!r}")


def check() -> None:
    for file in TOOLS.glob("*.py"):
        ast.parse(text(file), filename=str(file))
    defs = runpy.run_path(str(TOOLS / "AuthorGoldLevelDefinitions.py"))
    realized = runpy.run_path(str(TOOLS / "AuthorGoldRealizedGraphs.py"))
    map_script = runpy.run_path(str(TOOLS / "AuthorGoldLevelMap.py"))
    reopens = runpy.run_path(str(TOOLS / "ValidateGoldLevelAssets.py"))

    assert_equal(len(defs["ORDER"]), 17, "GoldDefinition数量")
    assert_equal(len(set(defs["ORDER"])), 17, "GoldDefinition名称唯一")
    assert_equal(len(defs["MESHES"]), 3, "Gold MeshSet数量")
    assert_equal(len(realized["DESCRIPTORS"]), 3, "金标准实际Spawner图数量")
    assert_equal(len(reopens["TEMPLATE_IDS"]), 12, "Foundation模板数量")
    assert_equal(len(reopens["SUBGRAPH_IDS"]), 7, "Foundation公共子图数量")
    assert_equal(len(reopens["BLUEPRINT_NAMES"]), 11, "PCG蓝图种类数")
    assert_equal(len(reopens["DEFINITIONS"]), 17, "重开校验Definition数量")
    assert_equal(
        {name for name, *_ in reopens["DEFINITIONS"]}, set(defs["ORDER"]),
        "创建脚本与独立重开定义清单一致"
    )
    assert_equal(
        {name for name, *_ in realized["DESCRIPTORS"]}, set(reopens["REALIZED_NAMES"]),
        "真正Spawner生成图与独立重开清单一致"
    )
    assert_equal(
        {name for _, name, *_ in map_script["SPECIFICATIONS"]},
        {
            "PCG_Canopy", "PCG_Rock", "PCG_Resource", "PCG_ManualLock",
            "PCG_MajorRoad", "PCG_MinorPath", "PCG_FieldFence",
            "PCG_Parcel", "PCG_Crops", "PCG_Gate", "PCG_HandPlacedBridge"
        },
        "金标准测试地图真实Actor清单"
    )
    assert set(bp for bp, *_ in map_script["SPECIFICATIONS"]).issubset(reopens["BLUEPRINT_NAMES"]), \
        "测试地图不能使用未生成或未验证的Blueprint（蓝图）"
    assert set(short for _, _, short, *_ in realized["DESCRIPTORS"]).issubset(defs["ORDER"]), \
        "实际图必须引用已登记的真实MeshSet Definition（网格集合定义）"
    assert all(based in reopens["TEMPLATE_IDS"] for _, based, *_ in realized["DESCRIPTORS"]), \
        "实际图模板必须属于12项Foundation Template"

    # C++ Editor生成器和独立Python清单名称必须一一对应，避免脚本引用不存在的蓝图资产。
    creator = text(PCG / "Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp")
    graph_writer = text(PCG / "Source/GamePlatformPCGEditor/Private/Authoring/PCGDevelopmentGraph.cpp")
    actor_code = text(PCG / "Source/GamePlatformPCG/Private/Actors/GamePlatformPCGActors.cpp")
    masks_header = text(PCG / "Source/GamePlatformPCG/Public/Services/GamePlatformPCGSpatialRules.h")
    masks_code = text(PCG / "Source/GamePlatformPCG/Private/Nodes/GamePlatformPCGSpatialRules.cpp")
    blueprints = set(re.findall(r'TEXT\("(BP_PCG_[^"]+)"\)', creator))
    assert_equal(blueprints, set(reopens["BLUEPRINT_NAMES"]), "平台蓝图生成器与验证清单一致")
    for name in reopens["TEMPLATE_IDS"]:
        symbol = "FGamePlatformPCGTemplateIds::" + name.removeprefix("TPL_")
        assert symbol in graph_writer, f"Foundation模板合同未被真实Editor创作代码引用：{name}"
    assert "SetGraphLocal" in actor_code and "BindPlacedActorGraph" in creator, \
        "放置器需要把Graph（图资产）实际绑定给原生PCGComponent（PCG组件）"
    assert "bFillInterior" in masks_header and "Mask.bFillInterior && InsidePolygon" in masks_code, \
        "闭合围栏不得被误识别成清空整个农田内部的实心排除"

    config = text(GAME_CONFIG)
    assert '(Path="/Game/Development/Foundation/PCG/Definitions")' in config, \
        "GamePlatformData主资产扫描漏了金标准定义目录"
    assert '+DirectoriesToNeverCook=(Path="/Game/Development/Foundation")' in config, \
        "正式Client/Server Cook不应打包Foundation开发地图和占位资产"
    pipeline = text(TOOLS / "RunGoldLevelAuthoring.ps1")
    for stage in ("01-FoundationGraphs", "02-VillageBlueprints", "03-GoldDefinitions",
                  "04-RealizedSpawnerGraphs", "05-GoldLevelMap", "06-FreshEditorReopen"):
        assert stage in pipeline, "真实UE资产生成/回读缺少阶段：" + stage
    assert "'-EnablePlugins=PythonScriptPlugin'" in pipeline, \
        "必须使用锁定UE引擎认可的复数EnablePlugins命令行参数"
    assert "'-Apply'" not in pipeline or "if (!$Apply)" in pipeline, \
        "默认模式必须只读，真实创建须明确Apply"

    print("PCG_GOLD_AUTHORING_STATIC_CONTRACTS_PASS")
    print("TEMPLATES=12 SUBGRAPHS=7 BLUEPRINTS=11 DEFINITIONS=17 REALIZED=3 PROF