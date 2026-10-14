#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 平台UI材质函数 Monolith 真实制作器：7项、逐项原生保存及断点续建。

所属：GamePlatformUI（通用UI材质函数）。仅由正式UE5.8编辑器的
Monolith material_query 创建/连线/保存。默认 inspect（离线只读）。
apply 需要 MATERIAL_LIBRARY_UI_AUTHOR=1，已存在资产仅在原生类型、输入输出、
磁盘包都通过核对后跳过。任何不确定状态立即停止，不删除、不重建、不覆盖。
每项真实操作响应由已有 BirdFlockMonolithProtocol 留存；本脚本的状态报告
仅记已观察的保存证据，不能代表 Shader、Cook、人工验收通过。
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
QUEUE = ROOT / "Docs/Implementation/Materials/MaterialLibraryUIActionQueue_V1.json"
RECIPES = ROOT / "Tools/Unreal/UI/MaterialLibraryUIMonolithRecipes_V1.json"
REPORT_DIR = ROOT / "Saved/Validation/MaterialLibraryUI/FunctionProduction"
PROTOCOL = ROOT / "Tools/Unreal/VFX/BirdFlock"
CONTENT = ROOT / "Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Content/Materials/Functions"
PREFIX = "/GamePlatformUI/Materials/Functions/MF_GP_UI_"


def load_queue():
    """校验来源、数量、动作序列和独立路径，禁止在制作阶段改变旧五项UI函数。"""
    source = json.loads(RECIPES.read_text(encoding="utf-8"))
    queue = json.loads(QUEUE.read_text(encoding="utf-8"))
    original = {entry["capabilityId"]: entry for entry in source["items"]}
    rows = queue.get("items", [])
    if len(rows) != 7 or len(original) != 7 or queue.get("expectedFunctions") != 7:
        raise RuntimeError("必须是恰好7项剩余UI原生函数")
    if len({row["capabilityId"] for row in rows}) != 7:
        raise RuntimeError("UI制作能力编号重复")
    if len({row["packagePath"] for row in rows}) != 7:
        raise RuntimeError("UI资产完整路径重复")
    expected = ["get_function_info", "create_material_function", "build_function_graph",
                "set_function_metadata", "save_material", "get_function_info"]
    for row in rows:
        cid = row["capabilityId"]
        recipe = original.get(cid)
        if recipe is None or recipe["assetPath"] != row["packagePath"]:
            raise RuntimeError("UI动作队列与源配方资产路径不一致：" + cid)
        if row["packagePath"] != PREFIX + row["name"]:
            raise RuntimeError("非法UI资产归属路径：" + cid)
        actions = row["operations"]
        if [op["action"] for op in actions] != expected:
            raise RuntimeError("Monolith动作与冻结配方不一致：" + cid)
        if actions[2]["parameters"]["clear_existing"] is not False:
            raise RuntimeError("禁止清空已有UI函数图：" + cid)
        if actions[2]["parameters"]["graph_spec"] != recipe["graphSpec"]:
            raise RuntimeError("当前动作图与正式配方发生漂移：" + cid)
        if any(op["parameters"]["asset_path"] != row["packagePath"] for op in actions):
            raise RuntimeError("UI动作跨越了当前函数路径：" + cid)
    return rows


def _strings(value):
    """按已核对的Monolith JSON响应提取Python stdout；结构未知则失败关闭。"""
    if isinstance(value, str):
        yield value
    elif isinstance(value, dict):
        for obj in value.values():
            yield from _strings(obj)
    elif isinstance(value, (tuple, list)):
        for obj in value:
            yield from _strings(obj)


def decode_marker(result):
    """必须看到由真实UE Python打印的明确标记，不能把RPC健康视为资产存在。"""
    for message in _strings(result):
        match = re.search(r"SSLM_UI_NATIVE_STATUS:(\{[^\r\n]*\})", message)
        if match:
            return json.loads(match.group(1))
    raise RuntimeError("Monolith未返回可核对的UE原生资产状态，禁止后续写入")


def native_status(editor_python, row):
    """UE同时核对注册表、物理uasset、原生类型与真实函数图输入输出。"""
    path = row["packagePath"]
    recipe = next(entry for entry in json.loads(RECIPES.read_text(encoding="utf-8"))["items"]
                  if entry["capabilityId"] == row["capabilityId"])
    pins = sorted(x["name"] for x in recipe["graphSpec"]["inputs"])
    outputs = sorted(x["name"] for x in recipe["graphSpec"]["outputs"])
    disk = CONTENT / (path.rsplit("/", 1)[-1] + ".uasset")
    script = (
        "import unreal,os,json\n"
        + "expected=" + repr(str(ROOT / "Game/DivineBeastsArena.uproject")) + "\n"
        + "actual=str(unreal.Paths.get_project_file_path())\n"
        + "assert os.path.normcase(os.path.realpath(actual)) == os.path.normcase(os.path.realpath(expected)),actual\n"
        + "assert str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'),'引擎版本不是5.8'\n"
        + "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'PIE未退出'\n"
        + "assert not unreal.AssetRegistryHelpers.get_asset_registry().is_loading_assets(),'资产注册表未就绪'\n"
        + "path=" + repr(path) + "\n"
        + "disk=" + repr(str(disk)) + "\n"
        + "reg=unreal.EditorAssetLibrary.does_asset_exist(path)\n"
        + "physical=os.path.isfile(disk)\n"
        + "if reg != physical: raise RuntimeError('注册表与物理UI函数资产不一致/半成品: '+path)\n"
        + "if not reg:\n"
        + " stale=[os.path.splitext(disk)[0]+ext for ext in ('.uexp','.ubulk','.umap') if os.path.isfile(os.path.splitext(disk)[0]+ext)]\n"
        + " assert not stale,('UI函数有未注册的侧车或异类原生包，拒绝覆盖',stale)\n"
        + "if reg:\n"
        + " fn=unreal.EditorAssetLibrary.load_asset(path)\n"
        + " assert isinstance(fn,unreal.MaterialFunction),'同名资产不是MaterialFunction'\n"
        + " expressions=unreal.MaterialEditingLibrary.get_material_function_expressions(fn)\n"
        + " inputs=sorted(str(n.get_editor_property('input_name')) for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionInput))\n"
        + " outputs=sorted(str(n.get_editor_property('output_name')) for n in expressions if isinstance(n,unreal.MaterialExpressionFunctionOutput))\n"
        + " assert inputs==" + repr(pins) + ",('已存在UI函数输入不符',inputs)\n"
        + " assert outputs==" + repr(outputs) + ",('已存在UI函数输出不符',outputs)\n"
        + "else:\n"
        + " inputs=[]\n"
        + " outputs=[]\n"
        + "print('SSLM_UI_NATIVE_STATUS:'+json.dumps({'asset':path,'exists':reg,'physical':physical,'inputs':inputs,'outputs':outputs},separators=(',',':')))\n"
    )
    return decode_marker(editor_python(script))


def main():
    parser = argparse.ArgumentParser(description="7个平台UI材质函数Monolith原生制作与回读")
    parser.add_argument("--mode", choices=("inspect", "apply"), default="inspect")
    parser.add_argument("--from-id", default=None, help="断点续建起始CapabilityId（含本项）")
    parser.add_argument("--limit", type=int, default=7, help="本次最多处理的函数数量")
    opt = parser.parse_args()
    rows = load_queue()
    if opt.limit < 1 or opt.limit > 7:
        raise ValueError("单次创建数量必须为1至7")
    if opt.from_id is not None:
        ids = [row["capabilityId"] for row in rows]
        if opt.from_id not in ids:
            raise ValueError("断点编号不属于当前7项UI清单：" + opt.from_id)
        rows = rows[ids.index(opt.from_id):]
    rows = rows[:opt.limit]
    for row in rows:
        print("UI_FUNCTION_AUTHOR_PLAN", row["capabilityId"], row["packagePath"])
    if opt.mode == "inspect":
        print("INSPECT_ONLY 未调用Monolith，未创建任何二进制资产")
        return
    if os.environ.get("MATERIAL_LIBRARY_UI_AUTHOR") != "1":
        raise RuntimeError("必须设置MATERIAL_LIBRARY_UI_AUTHOR=1授权本次原生UI函数制作")
    sys.path.insert(0, str(PROTOCOL))
    from BirdFlockMonolithProtocol import call, editor_python
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    name = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex
    report = REPORT_DIR / (name + ".json")
    data = {"schemaVersion": 1, "mode": "native_ue58_creation",
            "scope": "only_seven_new_GamePlatformUI_functions",
            "shader": "deferred", "cook": "deferred", "visualReview": "pending",
            "status": "started", "records": []}

    def checkpoint():
        report.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    checkpoint()
    try:
        for row in rows:
            record = {"id": row["capabilityId"], "path": row["packagePath"],
                      "status": "checking", "actions": []}
            data["records"].append(record)
            checkpoint()
            before = native_status(editor_python, row)
            record["preflight"] = before
            if before["exists"]:
                record["status"] = "already_exists_native_verified_preserved"
                checkpoint()
                continue
            for operation in row["operations"][1:5]:
                action = operation["action"]
                result = call("material_query", {"action": action,
                                                "params": operation["parameters"]})
                record["actions"].append({"id": operation["sequenceId"],
                                          "action": action, "resultReceived": bool(result)})
                checkpoint()
            result = call("material_query", {"action": "update_material_function",
                                            "params": {"asset_path": row["packagePath"]}})
            record["actions"].append({"action": "update_material_function",
                                      "resultReceived": bool(result)})
            checkpoint()
            call("material_query", {"action": "save_material",
                                    "params": {"asset_path": row["packagePath"],
                                               "only_if_dirty": True}})
            info = call("material_query", {"action": "get_function_info",
                                           "params": {"asset_path": row["packagePath"]}})
            if not info:
                raise RuntimeError("Monolith没有返回函数信息：" + row["packagePath"])
            after = native_status(editor_python, row)
            if not (after["exists"] and after["physical"]):
                raise RuntimeError("保存后原生函数或物理包未通过UE回读：" + row["packagePath"])
            record["readback"] = after
            record["status"] = "native_asset_saved_and_readback_shader_pending"
            checkpoint()
        data["status"] = "completed_native_only"
    except Exception as error:
        data["status"] = "failed_stop_without_retry"
        data["error"] = repr(error)
        if data["records"] and data["records"][-1]["status"] == "checking":
            data["records"][-1]["status"] = "uncertain_must_inspect_before_retry"
        raise
    finally:
        checkpoint()
        print("SSLM_UI_NATIVE_EXECUTION_EVIDENCE", report)
    print("SSLM_UI_NATIVE_COMPLETED", len(data["records"]),
          "FINAL_SHADER_COOK_VISUAL_PENDING")


if __name__ == "__main__":
    main()
