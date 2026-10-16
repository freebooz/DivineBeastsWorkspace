#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM新手村狼散布地图的只读采样清单。

UE5.8 Editor执行，不修改世界，不自动建立导航体积，也不启动PIE、Cook或任何编译。
World Partition未加载的Actor不能被此编辑器可见列表代表，结果必须标记覆盖不足。
"""
from __future__ import annotations
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"
OUTPUT = ROOT / "Saved/Validation/VillageWolf/MapInspection/ReadOnlyEditorActors.json"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def run():
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "仅允许真实UE5.8编辑器")
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    require(project == (ROOT / "Game/DivineBeastsArena.uproject").resolve(),
            "非SSLM正式工程，拒绝检查")
    require(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() == [],
            "已有未保存地图，禁止更换编辑器当前关卡")
    require(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() == [],
            "已有未保存其它资源，禁止打断其他作者")
    try:
        subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        require(subsystem and subsystem.get_game_world() is None,
                "PIE/模拟运行中不检查")
    except AttributeError:
        pass
    loaded = unreal.EditorLevelLibrary.load_level(MAP)
    require(loaded, "正式Village地图无法打开或没有加载到Editor：" + MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    counters = {
        "PlayerStart": 0, "NavMeshBoundsVolume": 0, "Landscape": 0,
        "PCG": 0, "Water": 0, "Wolf": 0,
    }
    starts = []
    nav_volumes = []
    for actor in actors:
        if not actor:
            continue
        name = actor.get_class().get_name()
        label = actor.get_actor_label()
        location = actor.get_actor_location()
        if actor.is_a(unreal.PlayerStart):
            counters["PlayerStart"] += 1
            starts.append({"label": label, "positionCm": [location.x, location.y, location.z]})
        if "NavMeshBoundsVolume" in name:
            counters["NavMeshBoundsVolume"] += 1
            nav_volumes.append({"label": label, "positionCm": [location.x, location.y, location.z]})
        if "Landscape" in name:
            counters["Landscape"] += 1
        if "PCG" in name or "PCG" in label:
            counters["PCG"] += 1
        if "Water" in name or "Water" in label:
            counters["Water"] += 1
        if "Wolf" in name or "Wolf" in label:
            counters["Wolf"] += 1
    report = {
        "Project": "SSLM", "Engine": str(unreal.SystemLibrary.get_engine_version()),
        "Map": MAP, "LoadedActorCount": len(actors), "Counts": counters,
        "PlayerStarts": starts, "NavMeshBoundsVolumes": nav_volumes,
        "EditorChanges": False, "CompileOrCook": False,
        "WorldPartitionUnloadedActorsIncluded": False,
        "ResultNotProofOfRuntimeWolfSpawn": True,
    }
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print("SSLM_VILLAGE_WOLF_MAP_READONLY", json.dumps(report, ensure_ascii=False))
run()
