#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 通用材质函数库原生资源 P0—P10 顺序生产入口。

仅调用既有UE5.8/Monolith真实制作者，不重新设计配方、不创建文本.uasset。
默认inspect；apply必须MATERIAL_LIBRARY_PRODUCTION=1。逐阶段独立Runner Python
子进程与完整输出报告，任一异常立即退出，不自动重放不确定的写操作。
用户指定最后统一Shader/蓝图全编译、Editor/Client/Server、Cook及人工签收，
因此本入口只承担当前请求的原生资源落盘与保存回读。
审核地图的空间布置只能对已安全打开的独立地图分别执行populate。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SURFACE = ROOT / "Tools/Unreal/Surface"
UI = ROOT / "Tools/Unreal/UI"
REPORT_DIR = ROOT / "Saved/Validation/MaterialLibrary/FullResourceProduction"

PHASES = (
    ("domain_functions", SURFACE / "RunMaterialLibraryNativeBatchesMonolith.py",
     ("--mode", "apply", "--from-stage", "functions")),
    ("material_layers", SURFACE / "RunMaterialLibraryNativeBatchesMonolith.py",
     ("--mode", "apply", "--from-stage", "layers")),
    ("review_stand_blueprint", SURFACE / "RunMaterialLibraryNativeBatchesMonolith.py",
     ("--mode", "apply", "--from-stage", "blueprint")),
    ("domain_review_materials", SURFACE / "RunMaterialLibraryNativeBatchesMonolith.py",
     ("--mode", "apply", "--from-stage", "domain_reviews")),
    ("ui_seven_functions", UI / "RunMaterialLibraryUIProductionMonolith.py",
     ("--mode", "apply")),
    ("ui_twelve_review_materials", SURFACE / "RunMaterialLibraryNativeBatchesMonolith.py",
     ("--mode", "apply", "--from-stage", "ui_reviews")),
    ("ui_widget", UI / "AuthorMaterialLibraryWidgetMonolith.py",
     ("--mode", "author")),
    ("ui_widget_material_brushes", UI / "BindMaterialLibraryUIReviewBrushesMonolith.py",
     ("--mode", "apply")),
    ("review_maps_four", SURFACE / "RunMaterialLibraryReviewMapsMonolith.py",
     ("--mode", "apply")),
)
SOURCE_OF_TRUTH = ROOT / "Docs/Implementation/Materials/MaterialCapabilityRegistry_V1.json"


def preflight_source():
    """静态核对201项合同及全部原生制作入口，没有任何资产写入。"""
    data = json.loads(SOURCE_OF_TRUTH.read_text(encoding="utf-8"))
    if data.get("candidateCount") != 201 or len(data.get("items", [])) != 201:
        raise RuntimeError("201项通用材质能力注册表不完整")
    if len({r["CapabilityId"] for r in data["items"]}) != 201:
        raise RuntimeError("材质能力身份存在重复")
    if len(PHASES) != 9:
        raise RuntimeError("原生资源生产阶段列表意外变化")
    for name, file, _ in PHASES:
        if not file.is_file():
            raise FileNotFoundError("原生制作脚本不存在：" + name + "：" + str(file))


def main():
    parser = argparse.ArgumentParser(description="SSLM P0—P10通用材质库原生资源顺序制作")
    parser.add_argument("--mode", choices=("inspect", "apply"), default="inspect")
    parser.add_argument("--resume-from", choices=[x[0] for x in PHASES],
                        default=PHASES[0][0])
    parser.add_argument("--batch-size", type=int, default=3, choices=(1, 2, 3, 4, 5))
    parser.add_argument("--max-phases", type=int, default=0,
                        help="0表示从断点执行到末尾；非零限定本次最多执行的阶段数")
    opt = parser.parse_args()
    preflight_source()
    if opt.max_phases < 0:
        raise ValueError("执行阶段数量不得为负")
    first = [x[0] for x in PHASES].index(opt.resume_from)
    selected = PHASES[first:first + opt.max_phases] if opt.max_phases else PHASES[first:]
    for name, script, args in selected:
        print("SSLM_MATERIAL_RESOURCE_PHASE_PLAN", name, script.relative_to(ROOT), " ".join(args))
    if opt.mode == "inspect":
        print("INSPECT_ONLY 尚无本次新增UE原生二进制资产证据")
        return
    if os.environ.get("MATERIAL_LIBRARY_PRODUCTION") != "1":
        raise RuntimeError("必须设置MATERIAL_LIBRARY_PRODUCTION=1授权运行正式资源制作")
    env = dict(os.environ)
    env.update({
        "MATERIAL_LIBRARY_NATIVE_AUTHOR": "1",
        "MATERIAL_LIBRARY_UI_AUTHOR": "1",
        "MATERIAL_LIBRARY_WIDGET_AUTHOR": "1",
        "MATERIAL_LIBRARY_WIDGET_BINDING": "1",
        "MATERIAL_LIBRARY_MAP_AUTHOR": "1",
    })
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    rid = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex
    report = REPORT_DIR / (rid + ".json")
    evidence = {"schemaVersion": 1, "status": "started",
                "project": "DivineBeastsWorkspace/Game/DivineBeastsArena.uproject",
                "scope": "only_native_assets_no_final_build",
                "editorCompile": "deferred_except_if_required_for_editor_start",
                "clientServerBuild": "deferred", "shader": "deferred",
                "cook": "deferred", "manualVisual": "pending", "phases": []}

    def checkpoint():
        report.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")

    checkpoint()
    try:
        for name, script, base_args in selected:
            args = [sys.executable, str(script), *base_args]
            if script.name == "RunMaterialLibraryNativeBatchesMonolith.py":
                args += ["--batch-size", str(opt.batch_size)]
            row = {"name": name, "argv": args[1:], "status": "started"}
            evidence["phases"].append(row)
            checkpoint()
            # 子进程执行真实Monolith且stdout/stderr通过Runner实时可见；最终状态由退出码决定。
            result = subprocess.run(args, cwd=str(ROOT), env=env, check=False)
            row["exitCode"] = result.returncode
            if result.returncode != 0:
                row["status"] = "failed_must_inspect_real_ue_assets_before_resume"
                raise RuntimeError("原生资源制作阶段失败：" + name + " code=" + str(result.returncode))
            row["status"] = "author_completed_requires_ue_asset_registry_recheck"
            checkpoint()
        evidence["status"] = "native_authoring_phases_returned_zero_not_final_qa"
    except Exception as error:
        evidence["status"] = "failed_stop_and_preserve_existing_assets"
        evidence["error"] = repr(error)
        raise
    finally:
        checkpoint()
        print("SSLM_FULL_RESOURCE_AUTHOR_EVIDENCE", report)
    print("SSLM_FULL_RESOURCE_PHASES_DONE", len(selected))


if __name__ == "__main__":
    main()
