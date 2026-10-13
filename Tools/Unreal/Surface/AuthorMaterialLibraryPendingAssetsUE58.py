#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 通用材质函数库的安全原生资源制作编排（最终统一编译之前）。

只在正式UE5.8 Editor内创建缺失MaterialFunction（60项）、PBR MaterialLayer（12项）
与共享Blend（1项）、项目审核Actor Blueprint（1项）。
不创建UI WBP（必须Monolith），不创建正式关卡，不改生产母版，不运行Shader重编译
与Cook。原生MaterialFunction作者内的UpdateMaterialFunction是编辑器节点一致性刷新，
不同于用户要求最后运行的统一Editor/Client/Server构建与Cook验证。
默认只读inspect；apply明确写入/保存真实UE资产；失败立即中止并写实际进度清单。
不会冒充跨平台全部201项已经实现。
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[3]
DOMAIN = ROOT / "Tools/Unreal/Surface/AuthorMaterialLibraryDomainUE58.py"
LAYERS = ROOT / "Tools/Unreal/Surface/AuthorMaterialLibraryLayersUE58.py"
BLUEPRINT = ROOT / "Tools/Unreal/Surface/AuthorMaterialLibraryStandBlueprintUE58.py"
OUTPUT = ROOT / "Game/Saved/Validation/MaterialLibrary/MaterialAssetAuthoringCheckpoint.json"


def check_project(unreal) -> None:
    """引擎进程、项目身份不符则绝不分发可写资源作者。"""
    current = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not current.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("资源作者必须运行于DivineBeastsWorkspace正式UE5.8主工程")
    if not str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."):
        raise RuntimeError("当前引擎不是锁定的Unreal Engine 5.8，拒绝创建资产")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor is None or editor.get_game_world() is not None:
        raise RuntimeError("当前编辑器处于PIE或无编辑上下文，禁止制作材质资源")


def record(payload) -> None:
    """记录已发生的实际执行结果；不产生虚构的Shader或Cook通过记录。"""
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temp = OUTPUT.with_suffix(".tmp")
    temp.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
    temp.replace(OUTPUT)


def launch_author(file: Path, args: list[str]) -> None:
    """不同作者各自用真实UE API，单一顺序运行防止同名包并发写入。"""
    if not file.is_file():
        raise FileNotFoundError("实际UE作者脚本未落盘：" + str(file))
    old = sys.argv
    try:
        sys.argv = [str(file), *args]
        runpy.run_path(str(file), run_name="__main__")
    finally:
        sys.argv = old


def main() -> None:
    """只制作资源阶段；Monolith UI和四类地图必须分别完成后再进入统一编译。"""
    ap = argparse.ArgumentParser(description="Material Library资产先制作，全部编译后置")
    ap.add_argument("--mode", choices=("inspect", "apply"), default="inspect")
    opts, _ = ap.parse_known_args()
    phases = [
        ("SurfaceFunctions", DOMAIN, ["--mode", opts.mode, "--owner", "Surface"]),
        ("VFXFunctions", DOMAIN, ["--mode", opts.mode, "--owner", "VFX"]),
        ("ProjectFunctions", DOMAIN, ["--mode", opts.mode, "--owner", "Project"]),
        ("MaterialLayers", LAYERS, ["--mode", opts.mode]),
        ("ReviewStandBlueprint", BLUEPRINT, ["--mode", opts.mode]),
    ]
    print("MATERIAL_LIBRARY_AUTHOR_MODE", opts.mode)
    print("ORDERED_PHASES", ", ".join(x[0] for x in phases))
    if opts.mode == "inspect":
        for label, target, arguments in phases:
            print("SOURCE_ONLY_PLAN", label, str(target.relative_to(ROOT)), " ".join(arguments))
        print("MISSING_AFTER_SOURCE_PHASE: 7 UI Monolith Functions; UI WBP; 4 standalone review maps.")
        print("NO_UE_ASSETS_CREATED_IN_INSPECT")
        return
    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as e:
        raise RuntimeError("本脚本只能在真正UE5.8 Editor运行") from e

    check_project(unreal)
    checkpoint = {
        "schemaVersion": 1,
        "dateUTC": datetime.now(timezone.utc).isoformat(),
        "project": str(unreal.Paths.get_project_file_path()),
        "assetPhase": "source-first native authoring",
        "overallStatus": "in_progress",
        "phases": [],
        "shaderAndCook": "deferred_for_final_user_requested_validation",
        "monolithUIAndWBP": "not_run_by_this_script",
        "blueprintFinalCompile": "deferred_for_final_validation",
    }
    record(checkpoint)
    for label, file, argv in phases:
        print("AUTHOR_PHASE_BEGIN", label, str(file))
        try:
            launch_author(file, argv)
        except Exception as error:
            checkpoint["phases"].append({
                "name": label, "status": "failed",
                "error": str(error),
                "noteZH": "可能已有部分原生资源保存；先盘点后续作，不得覆盖人工资源",
            })
            checkpoint["overallStatus"] = "blocked"
            record(checkpoint)
            raise
        checkpoint["phases"].append({"name": label, "status": "author_completed",
                                     "noteZH": "仅制作/保存原生资源；未运行最终统一编译、Cook或视觉签收"})
        record(checkpoint)
        print("AUTHOR_PHASE_COMPLETE", label)
    checkpoint["overallStatus"] = "native_author_phase_completed"
    checkpoint["remaining"] = [
        "Monolith制作7项UI函数及UI域真正UMaterial/WBP",
        "Monolith创建4个独立UWorld审核地图",
        "整理开发审核材质实例与正式项目接口后统一Shader/Editor/Client/Server/Cook",
    ]
    record(checkpoint)
    print("NATIVE_MATERIAL_ASSETS_AUTHORED_WITH_FINAL_COMPILE_DEFERRED")


if __name__ == "__main__":
    main()
