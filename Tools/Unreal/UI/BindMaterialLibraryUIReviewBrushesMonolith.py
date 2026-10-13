#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 12项UI审核材质绑定至真实开发Widget Image Brush（原生Monolith操作）。

依赖：原生12份MD_UI UMaterial和原生WBP均由既有Monolith作者保存。
仅修改专用 /DBAUIPack_Core/Development/MaterialLibrary/UI/ Widget，
不得改变项目正式UI主题或已有人工样品的不同材质绑定。
默认inspect只读；apply需要MATERIAL_LIBRARY_WIDGET_BINDING=1。
本阶段只设置Image Brush并保存；Shader/Widget正式编译与人工签收后置。
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
SPEC = ROOT / "Docs/Implementation/Materials/MaterialLibraryUIWidgetSpec_V1.json"
PROTOCOL = ROOT / "Tools/Unreal/VFX/BirdFlock"
ASSET = "/DBAUIPack_Core/Development/MaterialLibrary/UI/WBP_DBA_MaterialLibraryReview"
REPORT_DIR = ROOT / "Saved/Validation/MaterialLibraryUI/WidgetBrushBinding"


def entries():
    source = json.loads(SPEC.read_text(encoding="utf-8"))
    records = source.get("metadata", {}).get("pendingWidgetMaterialPaths", [])
    if source.get("name") != ASSET.rsplit("/", 1)[-1] or len(records) != 12:
        raise RuntimeError("12项UI Widget声明式规格不符")
    if len({i["nameEN"] for i in records}) != 12:
        raise RuntimeError("UI审核预览条目重复")
    for item in records:
        if not item["reviewMaterial"].startswith("/Game/Development/MaterialLibrary/UI/Materials/"):
            raise RuntimeError("UI审核材质越过开发隔离目录")
    return records


def success_marker(result):
    def flattened(value):
        if isinstance(value, str):
            yield value
        elif isinstance(value, dict):
            for entry in value.values():
                yield from flattened(entry)
        elif isinstance(value, (list, tuple)):
            for entry in value:
                yield from flattened(entry)
    for s in flattened(result):
        if "SSLM_UI_WIDGET_BRUSHES_SAVED:" in s:
            return s
    raise RuntimeError("Monolith未返回原生UI Brush更新保存标记，结果不确定")


def run_editor(editor_python, rows, read_only):
    """一次在真实UE内先检查12项依赖，拒绝任何部分缺失后再操作。"""
    program = [
        "import unreal,os,json",
        "real=str(unreal.Paths.get_project_file_path())",
        "expected=" + repr(str(ROOT / "Game/DivineBeastsArena.uproject")),
        "assert os.path.normcase(os.path.realpath(real))==os.path.normcase(os.path.realpath(expected)),real",
        "assert str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'),'必须使用UE5.8'",
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'PIE未退出'",
        "asset=" + repr(ASSET),
        "b=unreal.EditorAssetLibrary.load_asset(asset)",
        "assert isinstance(b,unreal.WidgetBlueprint),'真实UI审核WBP尚不存在'",
        "widget_tree=b.get_editor_property('widget_tree')",
        "assert widget_tree is not None,'WBP未包含实际WidgetTree'",
        "pairs=[]",
    ]
    for row in rows:
        path = row["reviewMaterial"]
        widget_name = "Preview_" + row["nameEN"]
        disk = ROOT / "Game/Content/Development/MaterialLibrary/UI/Materials" / (
            path.rsplit("/", 1)[-1] + ".uasset")
        program.extend([
            "m=unreal.EditorAssetLibrary.load_asset(" + repr(path) + ")",
            "assert isinstance(m,unreal.Material),'UI审核母版不存在或不是Material'",
            "assert m.get_editor_property('material_domain')==unreal.MaterialDomain.MD_UI,'审核样品未使用MD_UI域'",
            "assert os.path.isfile(" + repr(str(disk)) + "),'原生UI审核材质尚未落盘'",
            "w=widget_tree.find_widget(" + repr(widget_name) + ")",
            "assert isinstance(w,unreal.Image),'缺少真正Image样板：" + widget_name + "'",
            "brush=w.get_editor_property('brush')",
            "prior=brush.get_editor_property('resource_object')",
            "assert prior is None or prior.get_path_name()==m.get_path_name(),'已有样板绑定其他资源，拒绝覆盖'",
            "pairs.append((w,m))",
        ])
    if not read_only:
        program.extend([
            "for image,material in pairs:",
            " image.set_brush_from_material(material)",
            "assert unreal.EditorAssetLibrary.save_loaded_asset(b,only_if_is_dirty=False),'WBP保存失败'",
        ])
    program.extend([
        "for image,material in pairs:",
        " resource=image.get_editor_property('brush').get_editor_property('resource_object')",
        " assert resource is not None and resource.get_path_name()==material.get_path_name(),\'UI审查材质Brush绑定不正确\'",
        "print('SSLM_UI_WIDGET_BRUSHES_SAVED:'+json.dumps({'count':len(pairs),'readOnly':"
        + ("True" if read_only else "False") + "}))",
    ])
    return success_marker(editor_python("\n".join(program) + "\n"))


def main():
    parser = argparse.ArgumentParser(description="SSLM原生UI审核WBP 12个Image材质Brush绑定")
    parser.add_argument("--mode", choices=("inspect", "apply", "verify"), default="inspect")
    opts = parser.parse_args()
    rows = entries()
    for item in rows:
        print("WIDGET_BRUSH_PLAN", "Preview_" + item["nameEN"], item["reviewMaterial"])
    if opts.mode == "inspect":
        print("INSPECT_ONLY 未触发UE UI修改")
        return
    if opts.mode == "apply" and os.environ.get("MATERIAL_LIBRARY_WIDGET_BINDING") != "1":
        raise RuntimeError("必须设置MATERIAL_LIBRARY_WIDGET_BINDING=1后才能绑定专用UI样品")
    sys.path.insert(0, str(PROTOCOL))
    from BirdFlockMonolithProtocol import editor_python
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    name = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + "_" + uuid.uuid4().hex
    file = REPORT_DIR / (name + ".json")
    data = {"mode": opts.mode, "status": "started",
            "widget": ASSET, "count": 12, "shader": "deferred",
            "widgetCompile": "deferred", "manualVisual": "pending"}
    file.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    try:
        data["MonolithEvidence"] = run_editor(editor_python, rows, opts.mode == "verify")
        data["status"] = "ue_editor_readback_finished"
    except Exception as error:
        data["status"] = "failed_no_overwrite_or_unsafe_retry"
        data["error"] = repr(error)
        raise
    finally:
        file.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")
        print("SSLM_WIDGET_BRUSH_EVIDENCE", file)
    print("SSLM_WIDGET_BRUSH_PRODUCTION_DONE")


if __name__ == "__main__":
    main()
