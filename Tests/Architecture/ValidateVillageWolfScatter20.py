#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM狼群20只、30米间距的静态与理想平地几何测试。
本脚本不运行UE世界，不验证NavMesh、第三方骨骼、客户端Cook或真实游戏散布。
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private"
POLICY_PATH = SERVER / "Village/DivineBeastsVillageWolfSpawnPolicy.h"
SPAWNER_PATH = SERVER / "Village/DivineBeastsVillageWolfWorldSubsystem.cpp"
CONFIG_PATH = ROOT / "Game/Config/DefaultGame.ini"


def constant(source: str, name: str) -> float:
    """从正式C++策略读取值，不在测试中另写相互矛盾的固定目标。"""
    match = re.search(rf"\b{name}\s*=\s*([0-9]+(?:\.[0-9]+)?)", source)
    if not match:
        raise ValueError(f"找不到狼群配置常量 {name}")
    return float(match.group(1))


def validate() -> dict:
    source = POLICY_PATH.read_text(encoding="utf-8")
    server = SPAWNER_PATH.read_text(encoding="utf-8")
    config = CONFIG_PATH.read_text(encoding="utf-8-sig")
    limit = int(constant(source, "MaxAliveWolves"))
    spacing = constant(source, "MinimumSpawnSeparationCm")
    safe = constant(source, "PlayerStartSafetyRadiusCm")
    ring_count = int(constant(source, "RingCount"))
    directions = int(constant(source, "DirectionsPerRing"))
    first_radius = constant(source, "FirstRingRadiusCm")
    ring_step = constant(source, "RingStepCm")
    issues: list[str] = []
    if limit != 20 or spacing != 3000.0 or safe != 2200.0:
        issues.append("狼群上限20只、生成间距3000厘米、出生保护2200厘米不一致")
    if (ring_count, directions, first_radius, ring_step) != (8, 24, 4500.0, 3500.0):
        issues.append("扩展环形候选数量或距离与项目基线不一致")
    for name in ["HasSpawnSeparation", "WolfSpawnAnchors", "IsWolfAlive()",
                 "ProjectPointToNavigation", "LineTraceSingleByChannel",
                 "OverlapAnyTestByObjectType", "IsOutsidePlayerStart"]:
        if name not in source + server:
            issues.append(f"狼群代码缺少必要的权威保护：{name}")

    # 参考C++候选表达式的数学排列。在理想平地验证20点容量；
    # 实际地图若没有足够可行NavMesh位置，合法运行结果允许少于20只。
    positions = []
    for ordinal in range(limit):
        accepted = None
        for ring in range(ring_count):
            radius = first_radius + ring_step * ring
            for direction in range(directions):
                index = (direction * 7 + ordinal * 5 + ring * 3) % directions
                angle = 2 * math.pi * index / directions
                point = (radius * math.cos(angle), radius * math.sin(angle))
                if math.hypot(*point) < safe:
                    continue
                if all(math.dist(point, old) > spacing for old in positions):
                    accepted = point
                    break
            if accepted:
                break
        if accepted is None:
            issues.append(f"理想平地仅能容纳{len(positions)}只，目标20只")
            break
        positions.append(accepted)

    min_distance = min(
        (math.dist(first, second) for index, first in enumerate(positions)
         for second in positions[index + 1:]),
        default=float("inf"),
    )
    if len(positions) != 20 or min_distance <= spacing:
        issues.append("理想平地的最终生成点未全部达到严格>30米间距")

    if "bEnableVillageWolves=false" not in config or "bClientVisualsVerified=false" not in config:
        issues.append("Fab资源未到位前不应擅自开启狼生成/外观许可")

    return {
        "Passed": not issues,
        "MaximumAlive": limit,
        "MinimumSpawnSeparationMetersExclusive": spacing / 100,
        "IdealFlatGroundCandidates": len(positions),
        "IdealFlatGroundMinDistanceMeters": round(min_distance / 100, 3),
        "CandidateRingCount": ring_count,
        "DirectionsPerRing": directions,
        "MapAndNetworkValidation": "未执行，此脚本仅限理想平地与源码一致性",
        "Issues": issues,
    }


if __name__ == "__main__":
    result = validate()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result["Passed"] else 1)
