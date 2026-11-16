#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM项目世界PCG只读审计：真实UE5.8编辑器加载地图并检查PCG放置器、组件、注册和地形。
所属第三层世界内容消费审计；不创建资产、不执行Generate、不保存地图，不改变服务器权威。
脚本通过UE内置Python执行；以环境变量SSLM_PCG_AUDIT_MAP选择批准的正式地图。
输出到工作空间Saved/Validation/GamePlatformPCG，不产生PCG蓝图或伪造发布证明。
"""
from __future__ import annotations

import json
import os
from collections import Counter
from pathlib import Path

MAPS = {
    "/DBAWorldPack_Village/Maps/L_Village_Start": "Village",
    "/DBAWorldPack_MainArena/Maps/L_DBA_MainArena": "MainArena",
}
SCRIPT_VERSION = 1


def inspect_world() -> None:
    """独立编辑器打开一张已批准的实际地图；仅依据已加载Actor报告状态。"""
    import unreal as ue  # type: ignore

    # Windows的Path字符串显示可能恢复反斜杠，比较前须统一转为正斜杠。
    project = Path(str(ue.Paths.get_project_file_path()))
    if not str(project).replace("\\", "/").endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("当前UE编辑器并非SSLM唯一正式工程")
    package = os.environ.get("SSLM_PCG_AUDIT_MAP", "").strip()
    role = MAPS.get(package)
    if not role:
        raise RuntimeError("只允许审计当前注册的Village/MainArena正式地图")
    report_dir = project.parent.parent / "Saved/Validation/GamePlatformPCG/WorldAudit"
    report_dir.mkdir(parents=True, exist_ok=True)
    output = report_dir / ("PCG_" + role + "_NativeWorld.json")
    if not ue.EditorAssetLibrary.does_asset_exist(package):
        raise FileNotFoundError("正式地图在资产注册表中不存在：" + package)
    # 不提前LoadAsset(World)；UE5.8会因重复加载standalone UWorld导致地图切换时崩溃。
    if not ue.EditorLevelLibrary.load_level(package):
        raise RuntimeError("真实地图无法独立加载：" + package)

    actors = list(ue.EditorLevelLibrary.get_all_level_actors())
    pcg_actor_cls = getattr(ue, "GamePlatformPCGActorBase", None)
    director_cls = getattr(ue, "GamePlatformPCGWorldDirector", None)
    pcg_comp_cls = getattr(ue, "PCGComponent", None)
    if pcg_actor_cls is None or director_cls is None or pcg_comp_cls is None:
        raise RuntimeError("UE5.8缺少GamePlatformPCG平台反射类型，不能进行真实地图校验")
    by_class = Counter()
    pcg_actors = []
    directors = []
    with_pcg_component = 0
    landscape_count = 0
    for actor in actors:
        cls = actor.get_class().get_name()
        by_class[cls] += 1
        if "Landscape" in cls:
            landscape_count += 1
        if isinstance(actor, director_cls):
            directors.append(actor)
        if isinstance(actor, pcg_actor_cls):
            info = {"label": actor.get_actor_label(), "class": cls}
            for prop in ("domain_id", "world_stage", "seed", "graph", "profile_id", "source_id"):
                try:
                    info[prop] = str(actor.get_editor_property(prop))
                except Exception as error:
                    # 某些继承蓝图仅公开只读UProperty；必须区分“未读取”与空引用。
                    info[prop] = "UNREADABLE:" + str(type(error).__name__)
            pcg_actors.append(info)
        try:
            components = actor.get_components_by_class(pcg_comp_cls)
            if components:
                with_pcg_component += 1
        except Exception:
            # 不用“无组件”冒充可验证，分类统计与平台放置器结果仍以实际返回为准。
            raise RuntimeError("UE编辑器无法枚举原生PCGComponent：" + actor.get_name())

    director_result = None
    if len(directors) == 1:
        try:
            raw = directors[0].validate_participant_set()
            if isinstance(raw, str):
                valid = raw == ""
            elif isinstance(raw, tuple):
                valid = bool(raw) and raw[0] is True and not any(isinstance(i, str) and i for i in raw[1:])
            else:
                valid = raw is True
            director_result = {"valid": valid, "raw": str(raw)}
        except Exception as error:
            director_result = {"valid": False, "error": str(error)}

    result = {
        "schemaVersion": SCRIPT_VERSION,
        "status": "NativeWorldInspected_NotProductionAccepted",
        "role": role,
        "mapPackage": package,
        "totalLoadedActors": len(actors),
        "loadedActorClassCounts": dict(sorted(by_class.items())),
        "landscapeActorsLoaded": landscape_count,
        "actorsWithPCGComponentsLoaded": with_pcg_component,
        "platformPCGPlacersLoaded": len(pcg_actors),
        "platformPCGDirectorsLoaded": len(directors),
        "directorParticipantValidation": director_result,
        "pcgActors": pcg_actors,
        "limit": "仅读取当前加载的Actor；不证明WorldPartition未加载单元、HLOD、PCG实际生成、Bake、Cook或渲染性能",
    }
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(
        "SSLM_NATIVE_PCG_WORLD_AUDIT_PASS",
        role,
        "actors", len(actors),
        "pcg", len(pcg_actors),
        "directors", len(directors),
        "landscape", landscape_count,
        "components", with_pcg_component,
    )
    print("SSLM_NATIVE_PCG_WORLD_AUDIT_REPORT", output)


if __name__ == "__main__":
    inspect_world()
