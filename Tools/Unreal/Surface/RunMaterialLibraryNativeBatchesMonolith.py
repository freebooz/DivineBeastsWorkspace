#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 通用材质函数库真实UE5.8原生资源分批执行器（不进行最终编译）。

复用已经实施的原生UE脚本和项目唯一Monolith协议，不复制材质算法。
运行于能够连接127.0.0.1:9316/Monolith的Windows正式工程Runner。
默认inspect；apply要求MATERIAL_LIBRARY_NATIVE_AUTHOR=1，并且锁定UE5.8正式主工程。
逐批调用UE Python制作60项MF、12项ML+1项Blend、审查Actor BP、60份QA
及12份MD_UI QA。UI的其余7项独立UI函数必须先通过
RunMaterialLibraryUIProductionMonolith.py完成。
各UE脚本仅追加不存在的原生资源；已有资产验证失败直接终止。
单批超时、丢失连接或报错时不重试本次不确定的写操作；重连后先重新检查资产。
程序永不执行Shader最终编译、Client/Server编译、Cook、PIE或人工签收。
"""
from __future__ import annotations
import argparse
import json
import os
import re
import runpy
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SURFACE = ROOT / "Tools/Unreal/Surface"
UI = ROOT / "Tools/Unreal/UI"
PROTOCOL = ROOT / "Tools/Unreal/VFX/BirdFlock"
REPORT_DIR = ROOT / "Saved/Validation/MaterialLibrary/NativeBatchProduction"
STAGES = ("functions", "layers", "blueprint", "domain_reviews", "ui_reviews")
DOMAIN_COUNT = (("Surface", 46), ("VFX", 4), ("Project", 10))
SCRIPTS = {
    "functions": SURFACE / "AuthorMaterialLibraryDomainUE58.py",
    "layers": SURFACE / "AuthorMaterialLibraryLayersUE58.py",
    "blueprint": SURFACE / "AuthorMaterialLibraryStandBlueprintUE58.py",
    "domain_reviews": SURFACE / "AuthorMaterialLibraryDomainUE58.py",
    "ui_reviews": UI / "AuthorMaterialLibraryUIReviewUE58.py",
}


def plan(batch_size):
    """冻结计划由真实能力数量构成；同一序列索引允许重复执行并逐项验证。"""
    if batch_size not in (1, 2, 3, 4, 5):
        raise ValueError("单次Monolith原生制作数量只允许1～5")
    result = []
    for owner, count in DOMAIN_COUNT:
        for start in range(0, count, batch_size):
            result.append(("functions", owner, start, min(batch_size, count - start),
                           SCRIPTS["functions"],
                           ("--mode", "apply", "--owner", owner,
                            "--start-index", str(start), "--limit", str(min(batch_size, count-start)))))
    for start in range(0, 12, batch_size):
        n = min(batch_size, 12-start)
        result.append(("layers", "Surface", start, n, SCRIPTS["layers"],
                       ("--mode", "apply", "--start-index", str(start), "--limit", str(n))))
    result.append(("blueprint", "Project", 0, 1, SCRIPTS["blueprint"],
                   ("--mode", "apply")))
    for owner, count in DOMAIN_COUNT:
        for start in range(0, count, batch_size):
            n = min(batch_size, count-start)
            result.append(("domain_reviews", owner, start, n,
                           SCRIPTS["domain_reviews"],
                           ("--mode", "review", "--owner", owner,
                            "--start-index", str(start), "--limit", str(n))))
    for start in range(0, 12, batch_size):
        n = min(batch_size, 12-start)
        result.append(("ui_reviews", "UI", start, n, SCRIPTS["ui_reviews"],
                       ("--mode", "apply", "--start-index", str(start), "--limit", str(n))))
    return result


def _values(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for part in value.values():
            yield from _values(part)
    elif isinstance(value, (list, tuple)):
        for part in value:
            yield from _values(part)


def require_marker(response, marker, identity):
    """没有UE执行末尾标志就视为不确定终止，不靠Monolith服务在线来推断成功。"""
    for item in _values(response):
        if marker + ":" + identity in item:
            return
    raise RuntimeError("UE未回传批次完成标志："+identity+"；必须人工核实后续建")


def asset_contracts(stage, owner, offset, count):
    """从冻结配方计算本批应真正保存的资产；作者返回成功不等于资源落盘。"""
    author = runpy.run_path(str(SCRIPTS[stage]), run_name="_sslm_readonly_asset_contract")
    rows = []
    if stage in ("functions", "domain_reviews"):
        entries = [r for r in author["load_catalog"]() if r["owner"] == owner]
        for entry in entries[offset:offset + count]:
            if stage == "functions":
                target = author["asset_path"](entry)
                disk = str(author["disk_path"](entry))
                cls = "MaterialFunction"
                minimum = len(entry["pins"]) + len(entry["outputs"]) + 1
            else:
                target = author["REVIEW_ROOT"] + "/M_DBA_MaterialLibrary_" + entry["name"] + "_DomainReview"
                disk = str(ROOT / "Game/Content/Development/MaterialLibrary/Materials" / (target.rsplit("/", 1)[-1] + ".uasset"))
                cls = "Material"
                minimum = len(entry["pins"]) + 2
            rows.append((target, disk, cls, minimum))
    elif stage == "layers":
        entries = author["identities"]()[offset:offset + count]
        blend = author["BLEND"]
        rows.append((blend, str(author["DISK"] / (blend.rsplit("/", 1)[-1] + ".uasset")),
                     "MaterialFunctionMaterialLayerBlend", 5))
        for entry in entries:
            target = entry["materialLayerAssetPath"]
            rows.append((target, str(author["DISK"] / (target.rsplit("/", 1)[-1] + ".uasset")),
                         "MaterialFunctionMaterialLayer", 7))
    elif stage == "blueprint":
        target = author["ASSET"]
        rows.append((target, str(author["CONTENT"] / (target.rsplit("/", 1)[-1] + ".uasset")),
                     "Blueprint", 0))
    elif stage == "ui_reviews":
        for entry in author["load_specs"]()[offset:offset + count]:
            target = author["qa_path"](entry["name"])
            rows.append((target, str(author["QA_DISK"] / (target.rsplit("/", 1)[-1] + ".uasset")),
                         "Material", len(entry["inputs"]) + 2))
    if not rows or len(rows) != count + (1 if stage == "layers" else 0):
        raise RuntimeError("原生资产合同数量与批次不符：" + stage)
    return rows


def call_ue_script(editor_python, path, args, identity, stage, owner, offset, count):
    """在真实UE进程运行既有作者后，逐份核对原生类型、表达式与物理文件。"""
    contracts = asset_contracts(stage, owner, offset, count)
    script = (
        "import unreal,os,sys,runpy\n"
        "project=str(unreal.Paths.get_project_file_path())\n"
        "expected=" + repr(str(ROOT / "Game/DivineBeastsArena.uproject")) + "\n"
        "assert os.path.normcase(os.path.realpath(project))==os.path.normcase(os.path.realpath(expected)),project\n"
        "assert str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'),'必须使用UE5.8'\n"
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'不能在PIE中写资产'\n"
        "assert not unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets(),'AssetRegistry未就绪'\n"
        "previous=sys.argv[:]\n"
        "try:\n"
        " sys.argv=[" + repr(str(path)) + "," + ",".join(repr(x) for x in args) + "]\n"
        " runpy.run_path(" + repr(str(path)) + ",run_name='__main__')\n"
        "finally:\n"
        " sys.argv=previous\n"
        "contracts=" + repr(contracts) + "\n"
        "for asset,disk,cls,min_nodes in contracts:\n"
        " obj=unreal.EditorAssetLibrary.load_asset(asset)\n"
        " assert isinstance(obj,getattr(unreal,cls)),('原生类型不符',asset,cls)\n"
        " assert os.path.isfile(disk),('未保存真实uasset',disk)\n"
        " if min_nodes:\n"
        "  num=(unreal.MaterialEditingLibrary.get_num_material_expressions(obj) if cls=='Material'\n"
        "       else unreal.MaterialEditingLibrary.get_num_material_expressions_in_function(obj))\n"
        "  assert num>=min_nodes,('原生表达式数量不足',asset,num,min_nodes)\n"
        " if " + repr(stage) + "=='ui_reviews':\n"
        "  assert obj.get_editor_property('material_domain')==unreal.MaterialDomain.MD_UI,'UI审核材质域错误'\n"
        "print(" + repr("SSLM_NATIVE_BATCH_SAVED:" + identity + ":" + str(len(contracts))) + ")\n"
    )
    require_marker(editor_python(script), "SSLM_NATIVE_BATCH_SAVED", identity)


def main():
    parser = argparse.ArgumentParser(description="SSLM 60MF/12ML/Blueprint/QA UE5.8分批原生制作")
    parser.add_argument("--mode", choices=("inspect", "apply"), default="inspect")
    parser.add_argument("--from-stage", choices=STAGES, default="functions")
    parser.add_argument("--from-batch", type=int, default=0,
                        help="本阶段批次断点（0开始），只用于明确人工确认后的续建")
    parser.add_argument("--batch-size", type=int, default=3)
    parser.add_argument("--max-batches", type=int, default=0, help="0表示执行到所选阶段末尾")
    parser.add_argument("--through-final-stage", action="store_true",
                        help="默认只执行一个阶段；显式选择才跨阶段执行后续资产")
    options = parser.parse_args()
    if options.from_batch < 0 or options.max_batches < 0:
        raise ValueError("批次索引和处理数量不能为负")
    tasks = plan(options.batch_size)
    stages = STAGES[STAGES.index(options.from_stage):] if options.through_final_stage else (options.from_stage,)
    selected = [t for t in tasks if t[0] in stages]
    if options.from_batch >= len([t for t in selected if t[0] == options.from_stage]):
        raise ValueError("初始批次索引超过当前阶段的实际数量")
    indices = {stage: 0 for stage in STAGES}
    filtered = []
    for item in selected:
        ordinal = indices[item[0]]
        indices[item[0]] += 1
        if item[0] == options.from_stage and ordinal < options.from_batch:
            continue
        filtered.append((ordinal, *item))
    if options.max_batches:
        filtered = filtered[:options.max_batches]
    for ordinal, stage, owner, offset, count, file, args in filtered:
        if not file.is_file():
            raise FileNotFoundError("缺少真实原生作者源码：" + str(file))
        print("SSLM_NATIVE_BATCH_PLAN", stage, ordinal, owner,
              "offset", offset, "count", count, file.relative_to(ROOT))
    if options.mode == "inspect":
        print("INSPECT_ONLY 不调用Monolith或引擎；不生成任何.uasset")
        return
    if os.environ.get("MATERIAL_LIBRARY_NATIVE_AUTHOR") != "1":
        raise RuntimeError("需显式设置MATERIAL_LIBRARY_NATIVE_AUTHOR=1授权本次UE资源制作")
    sys.path.insert(0, str(PROTOCOL))
    from BirdFlockMonolithProtocol import editor_python
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    name = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex
    path = REPORT_DIR / (name + ".json")
    evidence = {"schemaVersion": 1, "status": "started",
                "scope": "native_authoring_no_shader_compile_or_cook",
                "shaderCompilation": "deferred", "cook": "deferred",
                "manualVisualSignoff": "pending", "batches": []}

    def checkpoint():
        path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")

    checkpoint()
    try:
        for ordinal, stage, owner, offset, count, script, args in filtered:
            identity = "%s_%s_%02d_%02d" % (stage, owner, ordinal, offset)
            row = {"stage": stage, "owner": owner, "ordinal": ordinal,
                   "offset": offset, "count": count,
                   "author": str(script.relative_to(ROOT)), "status": "in_progress"}
            evidence["batches"].append(row)
            checkpoint()
            call_ue_script(editor_python, script, args, identity, stage, owner, offset, count)
            row["status"] = "ue_native_assets_saved_and_type_checked_shader_pending"
            checkpoint()
        evidence["status"] = "batches_completed_not_final_verified"
    except Exception as error:
        evidence["status"] = "failed_stop_do_not_auto_retry"
        evidence["error"] = repr(error)
        if evidence["batches"] and evidence["batches"][-1]["status"] == "in_progress":
            evidence["batches"][-1]["status"] = "uncertain_inspect_saved_assets_before_resume"
        raise
    finally:
        checkpoint()
        print("SSLM_NATIVE_BATCH_EVIDENCE", path)
    print("SSLM_NATIVE_BATCH_RUN_DONE", len(filtered))


if __name__ == "__main__":
    main()
