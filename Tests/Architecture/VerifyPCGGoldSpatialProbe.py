#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PCG GoldLevel（程序化内容生成金标准关卡）空间验收源码前置检查。

只读审查“真正UE地图重开、已保存图空间掩码、地图来源与采样点”代码覆盖。
它绝不对未执行的G01～G16、实例化、导航或双端Cook报告运行通过。
"""
from __future__ import annotations

import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PCG = ROOT / "Game/Plugins/GamePlatform/World/GamePlatformPCG"
SOURCE = PCG / "Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldMapAuthoring.cpp"
COMMAND = PCG / "Source/GamePlatformPCGEditor/Private/Commands/GamePlatformPCGGoldAssetsCommandlet.cpp"
INVOKER = ROOT / "Tools/Unreal/PCG/RunGoldSpatialProbe.ps1"

EXPECTED_CASES = (
    "道路切林", "小径切林", "道路切田", "农门优先保留", "桥梁优先保留",
    "人工锁最高优先级", "农田内部作物不误排除", "森林非排除区保持可生成",
    "高优先级对象不被道路切除", "逆序登记仍有稳定优先级",
)
PROOF_CHECKS = (
    "LoadMap(Filename)",
    "ValidateParticipantSet(Error)",
    "CollectSpatialMasks(AllMasks, Error)",
    "Settings->SubjectPriority != Priority",
    "Settings->Masks.Num() != ExpectedMasks.Num()",
    "Saved->bFillInterior != Expected.bFillInterior",
    "Saved->Vertices.Num() != Expected.Vertices.Num()",
    "Saved->Vertices[Index]",
    "GetParticipantsForStage(Spec.Stage)",
    "Fence->bFillInterior",
    "GamePlatformPCGSpatialRules::Evaluate(",
)
FORBIDDEN_SUCCESSES = ("G01～G16正式验收通过", "PCG真实Spawner生成已完成", "双端Cook已完成")


def require(value: bool, message: str) -> None:
    if not value:
        raise AssertionError(message)


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8-sig")
    command = COMMAND.read_text(encoding="utf-8-sig")
    invoker = INVOKER.read_text(encoding="utf-8-sig")
    for case in EXPECTED_CASES:
        require(('TEXT("' + case + '")') in source, "缺少固定Gold空间取样案例：" + case)
    for check in PROOF_CHECKS:
        require(check in source, "只读空间探针缺少关键约束：" + check)
    require('Stage.Equals(TEXT("SpatialProbe")' in command,
            "UE Gold命令行必须真正路由到只读SpatialProbe阶段")
    require('GamePlatformPCGGoldMap::ProbeSpatial(Error)' in command,
            "UE Gold命令行的只读阶段未调用真实地图探针")
    require('-Stage=SpatialProbe' in invoker, "入口没有使用原生SpatialProbe命令")
    require("-NoCompile" in invoker, "只读空间探针不得隐式重编正在使用的UE编辑器")
    require("Get-Process UnrealEditor,UnrealEditor-Cmd" in invoker,
            "共享UE项目资产读取前必须检查并行编辑器进程")
    require("LastWriteTimeUtc" in invoker,
            "源码变更晚于现存DLL时必须拒绝使用旧二进制")
    for phrase in FORBIDDEN_SUCCESSES:
        require(phrase not in source, "源文件存在没有运行证据的虚假验收宣称：" + phrase)
    print("PCG_GOLD_SPATIAL_PROBE_SOURCE_PREFLIGHT_PASS")
    print(f"CASES={len(EXPECTED_CASES)} SOURCE_RULES={len(PROOF_CHECKS)}")
    print("NOTE=仅完成源码合同前置检查，仍需独立UE5.8进程真实执行。")


if __name__ == "__main__":
    main()
