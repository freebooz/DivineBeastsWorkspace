#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 四张独立审核地图Monolith制作器：仅创建、保存并回读真实.umap。

复用 MaterialLibraryMapActionQueue_V1.json（地图动作队列）。默认inspect只读。
apply只创建缺失新地图，不打开/覆盖用户已有地图。populate要求操作者已安全
打开指定Surface/VFX/Project审核地图，再调用现有Demo UE5.8作者布置审查资源。
仅允许实际Monolith编辑器制作，任何失败不删除、不清理、不自动重试写操作。
"""
from __future__ import annotations
import argparse
import json
import os
import re
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
QUEUE = ROOT / "Docs/Implementation/Materials/MaterialLibraryMapActionQueue_V1.json"
DEMO = ROOT / "Tools/Unreal/Surface/AuthorMaterialLibraryDemoUE58.py"
PROTOCOL = ROOT / "Tools/Unreal/VFX/BirdFlock"
REPORT_DIR = ROOT / "Saved/Validation/MaterialLibrary/MapProduction"
AREAS = ("Surface", "VFX", "Project", "UI")


def load_rows():
    """逐条锁定原四地图身份与Monolith不打开脏地图的安全约束。"""
    q = json.loads(QUEUE.read_text(encoding="utf-8"))
    rows = q.get("items", [])
    if q.get("count") != 4 or len(rows) != 4 or {r["area"] for r in rows} != set(AREAS):
        raise RuntimeError("原有四张开发审核地图任务队列发生变化")
    for row in rows:
        area = row["area"]
        path = row["path"]
        if not path.startswith("/Game/Development/MaterialLibrary/" + area + "/Maps/"):
            raise RuntimeError("审核地图越过正式Development目录：" + path)
        steps = row.get("steps", [])
        if len(steps) != 2 or steps[1].get("tool") != "create_empty_map":
            raise RuntimeError("未知的地图制作动作：" + area)
        params = steps[1]["params"]
        if params.get("path") != path or params.get("open") is not False or (
            params.get("dirty_policy") != "refuse"
        ):
            raise RuntimeError("原始地图队列不再满足禁止打开未保存世界的要求")
    return rows


def response_marker(data):
    def flatten(x):
        if isinstance(x, str):
            yield x
        elif isinstance(x, dict):
            for v in x.values():
                yield from flatten(v)
        elif isinstance(x, (list, tuple)):
            for v in x:
                yield from flatten(v)
    for value in flatten(data):
        m = re.search(r"SSLM_MAP_NATIVE_STATUS:(\{[^\r\n]*\})", value)
        if m:
            return json.loads(m.group(1))
    raise RuntimeError("Monolith未提供可核验的UE地图注册/文件状态，不继续创建")


def native_state(editor_python, row):
    """实际UE5.8 AssetRegistry与.umap文件双核对，不假定Monolith网关在线即资源完成。"""
    path = row["path"]
    disk = (ROOT / "Game/Content" / path.removeprefix("/Game/")).with_suffix(".umap")
    program = (
        "import unreal,os,json\n"
        "p=str(unreal.Paths.get_project_file_path())\n"
        "e=" + repr(str(ROOT / "Game/DivineBeastsArena.uproject")) + "\n"
        "assert os.path.normcase(os.path.realpath(p))==os.path.normcase(os.path.realpath(e)),p\n"
        "assert str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'),'必须使用UE5.8'\n"
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'PIE仍在运行'\n"
        "assert not unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets(),'注册表未就绪'\n"
        "path=" + repr(path) + "\n"
        "disk=" + repr(str(disk)) + "\n"
        "exists=unreal.EditorAssetLibrary.does_asset_exist(path)\n"
        "physical=os.path.isfile(disk)\n"
        "if exists != physical: raise RuntimeError('原生地图注册与物理文件不一致：'+path)\n"
        "if exists:\n"
        " w=unreal.EditorAssetLibrary.load_asset(path)\n"
        " assert isinstance(w,unreal.World),'同名包不是World'\n"
        "print('SSLM_MAP_NATIVE_STATUS:'+json.dumps({'path':path,'exists':exists,'disk':physical},separators=(',',':')))\n"
    )
    return response_marker(editor_python(program))


def main():
    parser = argparse.ArgumentParser(description="四张原生开发审核地图安全制作")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify", "populate"),
                        default="inspect")
    parser.add_argument("--area", choices=AREAS, default=None)
    opts = parser.parse_args()
    rows = load_rows()
    if opts.area:
        rows = [r for r in rows if r["area"] == opts.area]
    if opts.mode == "populate" and opts.area not in ("Surface", "VFX", "Project"):
        raise RuntimeError("只允许对已打开的Surface/VFX/Project单张地图进行布置")
    for row in rows:
        print("SSLM_MAP_PLAN", row["area"], row["path"])
    if opts.mode == "inspect":
        print("INSPECT_ONLY 未生成任何原生World")
        return
    if opts.mode in ("apply", "populate") and os.environ.get("MATERIAL_LIBRARY_MAP_AUTHOR") != "1":
        raise RuntimeError("先设置MATERIAL_LIBRARY_MAP_AUTHOR=1授权审核地图制作")
    sys.path.insert(0, str(PROTOCOL))
    from BirdFlockMonolithProtocol import call, editor_python
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    rid = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex
    report_path = REPORT_DIR / (rid + ".json")
    evidence = {"schemaVersion": 1, "scope": "development_only", "mode": opts.mode,
                "status": "started", "Shader": "deferred", "Cook": "deferred",
                "manualVisual": "pending", "records": []}

    def record():
        report_path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")

    record()
    try:
        for row in rows:
            result = {"area": row["area"], "path": row["path"], "state": "checking"}
            evidence["records"].append(result)
            record()
            before = native_state(editor_python, row)
            result["preflight"] = before
            if opts.mode == "verify":
                if not before["exists"]:
                    raise RuntimeError("正式审核.umap尚不存在：" + row["path"])
                result["state"] = "native_umap_verified_no_visual_inference"
            elif opts.mode == "populate":
                if not before["exists"]:
                    raise RuntimeError("不能向尚未创建的审核地图放置资产")
                # 当前打开世界必须为用户指定审核地图；不调用任何自动关卡切换。
                source = (
                    "import sys,runpy,unreal\n"
                    "w=unreal.EditorLevelLibrary.get_editor_world()\n"
                    "assert w and w.get_path_name().split('.')[0]=="
                    + repr(row["path"]) + ",'必须先安全打开选定独立审核地图'\n"
                    "old=sys.argv[:]\n"
                    "try:\n"
                    " sys.argv=[" + repr(str(DEMO)) + ",'--mode','apply','--group',"
                    + repr(row["area"]) + "]\n"
                    " runpy.run_path(" + repr(str(DEMO)) + ",run_name='__main__')\n"
                    "finally:\n"
                    " sys.argv=old\n"
                    "print('SSLM_MAP_POPULATE_COMPLETE')\n"
                )
                editor_python(source)
                result["state"] = "scene_author_executed_final_visual_pending"
            elif before["exists"]:
                result["state"] = "existing_native_map_preserved"
            else:
                # 直接复用既有两步队列的原生Monolith建图动作。
                call("editor_query", {"action": "create_empty_map",
                                      "params": row["steps"][1]["params"]})
                after = native_state(editor_python, row)
                if not after["exists"] or not after["disk"]:
                    raise RuntimeError("创建后.umap未真实保存并回读：" + row["path"])
                result["readback"] = after
                result["state"] = "native_umap_saved_scene_pending"
            record()
        evidence["status"] = "native_phase_complete"
    except Exception as error:
        evidence["status"] = "failed_do_not_recreate"
        evidence["error"] = repr(error)
        if evidence["records"] and evidence["records"][-1]["state"] == "checking":
            evidence["records"][-1]["state"] = "uncertain_check_manually_before_retry"
        raise
    finally:
        record()
        print("SSLM_MAP_NATIVE_EVIDENCE", report_path)
    print("SSLM_MAP_NATIVE_PHASE_DONE", len(evidence["records"]))


if __name__ == "__main__":
    main()
