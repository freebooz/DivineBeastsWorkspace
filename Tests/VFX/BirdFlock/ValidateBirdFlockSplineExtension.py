#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群样条扩展完整性门禁：真实Git/源文件/配置/资产存在性，和UE真验证严格区分。

--source：当前代码+资源作者输入的静态检查，不需要UE Editor。
--strict：要求全部真实UE资产已创建、相关编译/真图回读与人工审核证据齐全；不足退出非零。
结果保存至Saved/Validation/BirdFlockSplineFlight/StaticExtensionGate.json。
不生成.uasset或冒充通过的GPU/人工报告，不改正式项目配置。
"""
import argparse
import ast
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SAVED=ROOT/"Saved/Validation/BirdFlockSplineFlight"
SOURCE=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock"
CLIENT=ROOT/"Game/Plugins/DivineBeasts/DBAClient"
FORMAL=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock"

NEEDED_SOURCE=[
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Public/Environment/BirdFlock/GamePlatformBirdFlockSplineMath.h",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Environment/BirdFlock/GamePlatformBirdFlockSplineMath.cpp",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Public/Environment/DivineBeastsBirdFlockFlightSettings.h",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockFlightSettings.cpp",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Public/Environment/DivineBeastsBirdFlockBatchComponent.h",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockBatchComponent.cpp",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Public/Environment/DivineBeastsBirdFlockRouteActor.h",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockRouteActor.cpp",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Public/UI/DivineBeastsBirdFlockDemoWidget.h",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/UI/DivineBeastsBirdFlockDemoWidget.cpp",
 "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsDemoClient/Private/Demo/DivineBeastsDemoBirdControls.cpp",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockSplineProgress.hlsl",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockSplineFollow.hlsl",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockBoidsSteering.hlsl",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/BirdFlock/BirdFlockWingState.hlsl",
 "Tools/Unreal/VFX/BirdFlock/ExtendBirdFlockSplineDefinitions.py",
 "Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockSplineReviewAssets.py",
 "Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockSplineNiagaraMonolith.py",
 "Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockSplineDemoUI.py",
 "Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockWingLODAssets.py",
 "Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockHabitatScenes.py",
 "Docs/Implementation/BirdFlockSplineFlight/BirdFlockDemoUISpec.json",
]
REQUIRED_REAL_ASSETS=[
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockSplineProgress.uasset",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockSplineFollow.uasset",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockBoidsSteering.uasset",
 "Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock/Niagara/NM_GP_BirdFlockWingState.uasset",
 "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock/Blueprints/BP_DBA_BirdFlockSplineRoute.uasset",
 "Game/Content/Development/BirdFlock/Maps/L_DBA_BirdFlockSplineReview.umap",
 "Game/Content/Development/PluginDemos/UI/WBP_DBA_PluginDemoBirdFlock.uasset",
]

REQUIRED_RUNTIME_EVIDENCE=[
 "NiagaraSplineAuthoring.json",
 "WorldDefinitionsAuthoring.json",
 "SplineSceneAuthoring.json",
 "BirdDemoUIAuthoring.json",
 "EditorBuildFinal.json",
 "ClientBuildFinal.json",
 "ServerBuildFinal.json",
 "ActualGpuReviewFinal.json",
 "ServerContainerValidationFinal.json",
 "ClientContainerValidationFinal.json",
 "ManualReviewSigned.json"
]


def check(cond,name,reason,results):
    results.append({"name":name,"status":"pass" if cond else "fail",
                    "reason":reason if not cond else ""})


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--source",action="store_true")
    parser.add_argument("--strict",action="store_true")
    args=parser.parse_args()
    report={"checks":[],"required_real_ue_assets":[],"required_final_evidence":[],
            "source_status":"pending","production_status":"not_validated",
            "readme":"源码扫描不证明真实UE Niagara、GPU效果、Shader或人工审美验收。"}
    git_sha=subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT).decode().strip()
    report["git_head"]=git_sha
    missing=[path for path in NEEDED_SOURCE if not (ROOT/path).is_file()]
    check(not missing,"all_required_source_present",str(missing),report["checks"])
    scripts=[ROOT/x for x in NEEDED_SOURCE if x.endswith(".py") and (ROOT/x).is_file()]
    ast_errors=[]
    for script in scripts:
        try:ast.parse(script.read_text(encoding="utf-8-sig"),filename=str(script))
        except (SyntaxError,UnicodeError) as error:ast_errors.append(f"{script.relative_to(ROOT)}: {error}")
    check(not ast_errors,"python_authoring_scripts_parse",str(ast_errors),report["checks"])
    shader_errors=[]
    for hlsl in SOURCE.glob("BirdFlock*.hlsl"):
        text=hlsl.read_text(encoding="utf-8-sig")
        if hlsl.name in {Path(x).name for x in NEEDED_SOURCE if x.endswith(".hlsl")}:
            if "#if GPU_SIMULATION" not in text or "#endif" not in text:
                shader_errors.append(hlsl.name+" missing GPU_SIMULATION wrapper")
    check(not shader_errors,"gpu_hlsl_sources_declared",str(shader_errors),report["checks"])
    demo_src=(CLIENT/"Source/DivineBeastsDemoClient/Private/Demo/DivineBeastsDemoBirdControls.cpp")
    if demo_src.is_file():
        demo_text=demo_src.read_text(encoding="utf-8-sig")
        check("AGamePlatformBirdFlockActor" not in demo_text and "GetActualBirdCount" in demo_text and
              "return INDEX_NONE;" in demo_text,"demo_excludes_old_ism_and_false_gpu_count",
              "演示仍使用旧ISM或用请求数冒充真实GPU粒子数",report["checks"])
    src=(CLIENT/"Source/DivineBeastsPresentationClient/Private/Environment/DivineBeastsBirdFlockPresentation.cpp")
    if src.is_file():
        txt=src.read_text(encoding="utf-8-sig")
        check("bEnableVerifiedGPUFlight" in txt and "StartAmbientBirdFlockForOwnerAdvanced" in txt,
              "advanced_gpu_feature_flag_closed_by_default",
              "正式高级功能缺少验证门禁",report["checks"])
    config=(ROOT/"Game/Config/DefaultEngine.ini").read_text(encoding="utf-8-sig")
    check("GameDefaultMap=/DBAFrontEndPack/Maps/L_DBA_FrontEnd" in config,
          "production_startup_map_unchanged","正式启动地图不符合既有基线",report["checks"])
    ui=ROOT/"Docs/Implementation/BirdFlockSplineFlight/BirdFlockDemoUISpec.json"
    if ui.is_file():
        data=json.loads(ui.read_text(encoding="utf-8"))
        check(data.get("parentClass")=="/Script/DivineBeastsDemoClient.DivineBeastsBirdFlockDemoWidget",
              "ui_authored_parent_correct","UI父类不符合C++类型身份",report["checks"])

    for asset in REQUIRED_REAL_ASSETS:
        present=(ROOT/asset).is_file()
        report["required_real_ue_assets"].append({"path":asset,"present":present,
            "status":"present_needs_ue_readback" if present else "missing_not_authored"})

    for filename in REQUIRED_RUNTIME_EVIDENCE:
        path=SAVED/filename
        exists=path.is_file()
        report["required_final_evidence"].append({"path":str(path.relative_to(ROOT)).replace("\\","/"),
            "present":exists,"status":"present_unverified" if exists else "not_run"})

    report["source_status"]="source_checks_passed" if all(x["status"]=="pass" for x in report["checks"]) else "source_checks_failed"
    all_real=all(x["present"] for x in report["required_real_ue_assets"])
    all_evidence=all(x["present"] for x in report["required_final_evidence"])
    report["production_status"]="requires_native_ue_and_human_verification"
    SAVED.mkdir(parents=True,exist_ok=True)
    dest=SAVED/"StaticExtensionGate.json"
    dest.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"source_status":report["source_status"],
        "static_passed":sum(x["status"]=="pass" for x in report["checks"]),
        "static_failed":sum(x["status"]!="pass" for x in report["checks"]),
        "unreal_assets_missing":sum(not x["present"] for x in report["required_real_ue_assets"]),
        "runtime_evidence_missing":sum(not x["present"] for x in report["required_final_evidence"]),
        "result_path":str(dest.relative_to(ROOT)).replace("\\","/")},ensure_ascii=False))
    if report["source_status"]!="source_checks_passed":raise SystemExit(2)
    if args.strict and (not all_real or not all_evidence):
        raise SystemExit(3)


if __name__=="__main__":
    main()
