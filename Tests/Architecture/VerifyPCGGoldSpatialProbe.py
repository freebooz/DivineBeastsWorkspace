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
ACTOR = PCG / "Source/GamePlatformPCG/Private/Actors/GamePlatformPCGActors.cpp"
BINDER = PCG / "Source/GamePlatformPCGEditor/Private/Authoring/GamePlatformPCGEditorLibrary.cpp"
DETERMINISM = ROOT / "Tools/Unreal/PCG/RunGoldDeterminism.ps1"

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
    actor = ACTOR.read_text(encoding="utf-8-sig")
    binder = BINDER.read_text(encoding="utf-8-sig")
    determinism = DETERMINISM.read_text(encoding="utf-8-sig")
    for case in EXPECTED_CASES:
        require(('TEXT("' + case + '")') in source, "缺少固定Gold空间取样案例：" + case)
    for check in PROOF_CHECKS:
        require(check in source, "只读空间探针缺少关键约束：" + check)
    require('Stage.Equals(TEXT("SpatialProbe")' in command,
            "UE Gold命令行必须真正路由到只读SpatialProbe阶段")
    require('GamePlatformPCGGoldMap::ProbeSpatial(Error)' in command,
            "UE Gold命令行的只读阶段未调用真实地图探针")
    require("ValidateSet('SpatialProbe','GeneratePreview')" in invoker, "入口没有明确限制只读探针或受控生成预览阶段")
    require("('-Stage='+$Stage)" in invoker, "入口没有向原生命令传递所选择的固定阶段")
    require('Stage.Equals(TEXT("GeneratePreview")' in command, "UE Gold命令行没有真实生成预览阶段")
    require("GamePlatformPCGGoldMap::PreviewGenerated(Error)" in command, "UE Gold命令行没有调用实际生成预览")
    require("-NoCompile" in invoker, "只读空间探针不得隐式重编正在使用的UE编辑器")
    require("Get-Process UnrealEditor,UnrealEditor-Cmd" in invoker,
            "共享UE项目资产读取前必须检查并行编辑器进程")
    require("LastWriteTimeUtc" in invoker,
            "源码变更晚于现存DLL时必须拒绝使用旧二进制")
    require("Source/GamePlatformPCG'" in invoker and
            "Source/GamePlatformPCGEditor'" in invoker and
            "Get-ChildItem -LiteralPath $check.Source -Recurse -File" in invoker,
            "G09预览必须核对运行/编辑器两模块全部源码的编译新鲜度，不能只检查Gold命令")
    for phrase in FORBIDDEN_SUCCESSES:
        require(phrase not in source, "源文件存在没有运行证据的虚假验收宣称：" + phrase)
    require("PrepareMasks(AllMasks, AllPrepared)" in source and
            "PrepareMasks(CropMasks, CropPrepared)" in source and
            "EvaluatePrepared(" in source,
            "实际PCG生成预览应对每批掩码预处理一次，禁止逐实例重复审查多边形面积")
    require("CleanupLocalImmediate(" in source and "CancelGeneration()" in source,
            "实际预览缺少取消/清理临时PCG资源的安全路径")
    require("!Component->bGenerated || Component->IsGenerating()" in source and
            "FPlatformTime::Seconds() - Start < 60.0" in source,
            "实际预览必须等待排队任务变为bGenerated，而非仅在IsGenerating初始true时等待")
    require("$process.WaitForExit(420000)" in invoker and "$process.Kill()" in invoker,
            "独立UE编辑器初始化不能无界等待，超时只能终止本进程")
    require('if (!ProbeSpatial(Error))' in source,
            "GeneratePreview必须先验证已保存的地图/图掩码一致，不能绕过SpatialProbe启动生成")
    require("PCGComponent->Seed = Seed;" in actor and
            "Actor->PCGComponent->Seed = Actor->Seed;" in binder,
            "Editor放置器Seed未同步官方PCGComponent，G09同种子复现合同无法成立")
    require("Get-FileHash" in invoker and "$hashes[$asset]" in invoker,
            "真实PCG预览必须确保Gold地图与三份Graph资源运行前后SHA-256未被改写")
    require("CanonicalInstances.Sort()" in source and "sha1-ism-mm-v1:" in source,
            "实际ISM预览缺少与迭代顺序无关的稳定指纹")
    require("SeenComponents.Contains(ISM)" in source and "Resource->GetOuter() != Component" in source,
            "输出审计不能重复统计Managed Resource或接受其它来源组件")
    require("for ($index = 1; $index -le 2; $index++)" in determinism and
            "GOLD_G09_TWO_FRESH_EDITORS_DETERMINISM_PASS" in determinism and
            "$first.Fingerprint -ne $second.Fingerprint" in determinism,
            "G09必须调用两个真实独立UE进程并严格比较三份网格输出指纹，不得只比Seed")
    require("GP_PCG_SPATIAL_PROBE_PASS" in source and
            "GP_PCG_GENERATE_PREVIEW_PASS" in source and
            "GP_PCG_PREVIEW_RESULT" in source and
            "GP_PCG_PREVIEW_RESULT" in determinism,
            "Windows终端编码可能导致中文日志不匹配，必须使用ASCII成功标志和实例指纹字段")
    require("GP_PCG_SPATIAL_PROBE_PASS" in invoker and
            "GP_PCG_GENERATE_PREVIEW_PASS" in invoker,
            "UE入口必须验证本阶段机器可解析的明确成功标志")
    spatial_scope = source.split("bool GamePlatformPCGGoldMap::ProbeSpatial", 1)[1].split(
        "bool GamePlatformPCGGoldMap::PreviewGenerated", 1)[0]
    require(spatial_scope.rfind("GP_PCG_SPATIAL_PROBE_PASS") < spatial_scope.rfind("return true;"),
            "只读空间探针成功日志必须先于函数返回，不得位于无法执行的死代码中")
    print("PCG_GOLD_SPATIAL_PROBE_SOURCE_PREFLIGHT_PASS")
    print(f"CASES={len(EXPECTED_CASES)} SOURCE_RULES={len(PROOF_CHECKS)}")
    print("NOTE=仅完成源码合同前置检查，仍需独立UE5.8进程真实执行。")


if __name__ == "__main__":
    main()
