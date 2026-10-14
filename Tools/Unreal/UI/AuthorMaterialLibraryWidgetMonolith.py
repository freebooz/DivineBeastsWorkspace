#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""《神兽联盟》通用材质库项目UI审核Widget蓝图Monolith原生作者。

只在用户授权的正式UE5.8编辑器启动、Monolith可用和C++父类反射已存在时
使用Monolith ui.build_ui_from_spec真正创建Widget蓝图。默认inspect只读。
审核规格为12个平台UI函数样品、返回按钮、真实操作反馈、滚动网格；
仅描述样例Image占位，不提前绑定尚未生成的UI材质Brush。
禁止普通Python创建假的WBP、覆盖已存在的WBP、自动回退父类为UUserWidget。
compile_widget与Shader、Cook、PIE、GPU和人工审核统一留至最终阶段。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SPEC=ROOT/"Docs/Implementation/Materials/MaterialLibraryUIWidgetSpec_V1.json"
ASSET="/DBAUIPack_Core/Development/MaterialLibrary/UI/WBP_DBA_MaterialLibraryReview"
PARENT="/Script/DivineBeastsDemoClient.DivineBeastsDemoHubWidget"
REPORT=ROOT/"Saved/Validation/MaterialLibraryUI/WidgetAuthoring_20261013.json"
BIRD_PROTOCOL=ROOT/"Tools/Unreal/VFX/BirdFlock"
REQUIRED={"RootCanvas","HeaderText","ReturnButton","StatusText",
          "OverlayFeedbackText","ReviewScroll","UIReviewImageCards"}


def visit(node):
    """递归收集声明式控件树全部唯一标识，避免Monolith合并同名控件。"""
    if isinstance(node, dict):
        if isinstance(node.get("id"),str):yield node["id"]
        for child in node.get("children",[]):yield from visit(child)


def source_spec():
    """确认为第三层项目UI、原生演示父类和十二种与机器清单相符的不同UI样板。"""
    data=json.loads(SPEC.read_text(encoding="utf-8"))
    if data.get("version")!=1 or data.get("parentClass")!=PARENT or data.get("name")!=ASSET.rsplit("/",1)[-1]:
        raise RuntimeError("材质库Widget名称或C++父类不符合现有DemoHub合同")
    if data.get("metadata",{}).get("assetDirectory")!=ASSET.rsplit("/",1)[0]:
        raise RuntimeError("材质审核Widget必须放在既有DBAUIPack_Core项目表现内容包")
    ids=list(visit(data["rootWidget"]))
    if len(ids)!=len(set(ids)) or not REQUIRED.issubset(ids):
        raise RuntimeError("UI树存在重复控件或缺少必须由C++演示逻辑绑定的返回/反馈控件")
    planned=data.get("metadata",{}).get("pendingWidgetMaterialPaths",[])
    if len(planned)!=12 or len({x["nameEN"] for x in planned})!=12:
        raise RuntimeError("未包含12种独立的GamePlatformUI通用材质审查卡片")
    for row in planned:
        if not row["reviewMaterial"].startswith("/Game/Development/MaterialLibrary/UI/Materials/"):
            raise ValueError("UI审查材质未隔离到Development目录："+row["nameEN"])
        if "Preview_"+row["nameEN"] not in ids:
            raise ValueError("UI样板没有真正Image控件："+row["nameEN"])
    return data,ids


def read_ue_widget_state(result):
    """只有编辑器打印的真实状态能决定复用/创建；不能把MCP健康当作资产。"""
    def all_strings(obj):
        if isinstance(obj,str):yield obj
        elif isinstance(obj,dict):
            for value in obj.values():yield from all_strings(value)
        elif isinstance(obj,(list,tuple)):
            for value in obj:yield from all_strings(value)
    for source in all_strings(result):
        found=re.search(r"SSLM_WBP_STATE:(EXISTS|MISSING)",source)
        if found:return found.group(1)
    raise RuntimeError("Monolith没有返回正式UE5.8 Widget原生状态，拒绝创建")


def main():
    """只生成本项目新WBP；有同名蓝图直接拒绝覆盖，留待只读核验。"""
    parser=argparse.ArgumentParser(description="材质库十二UI样板真实Monolith Widget蓝图作者")
    parser.add_argument("--mode",choices=("inspect","author"),default="inspect")
    options=parser.parse_args()
    spec,ids=source_spec()
    if options.mode=="inspect":
        print(json.dumps({"status":"source_spec_only",
                          "assetPath":ASSET,"parent":PARENT,
                          "widgetNodeCount":len(ids),"uiMaterialCards":12,
                          "originalWidgetCreated":False,
                          "uiCompilation":"deferred_to_final_batch"},ensure_ascii=False))
        return
    if os.environ.get("MATERIAL_LIBRARY_WIDGET_AUTHOR")!="1":
        raise RuntimeError("需显式设置MATERIAL_LIBRARY_WIDGET_AUTHOR=1以授权真实UI蓝图首次创建")
    sys.path.insert(0,str(BIRD_PROTOCOL))
    from BirdFlockMonolithProtocol import call,editor_python
    # 共享项目Monolith原生协议已有响应留证和失败中止规则，此处不新建MCP网关。
    physical=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/Development/MaterialLibrary/UI/WBP_DBA_MaterialLibraryReview.uasset"
    preflight=(
        "import unreal,os\n"
        "project=str(unreal.Paths.get_project_file_path())\n"
        "expected="+repr(str(ROOT/"Game/DivineBeastsArena.uproject"))+"\n"
        "assert os.path.normcase(os.path.realpath(project))==os.path.normcase(os.path.realpath(expected)),project\n"
        "assert str(unreal.SystemLibrary.get_engine_version()).startswith('5.8.'),'必须UE5.8'\n"
        "assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None,'PIE未退出'\n"
        "parent=unreal.load_class(None,"+repr(PARENT)+")\n"
        "assert parent is not None,'必须加载正式DivineBeastsDemoClient C++父类'\n"
        "asset="+repr(ASSET)+"\n"
        "disk="+repr(str(physical))+"\n"
        "exists=unreal.EditorAssetLibrary.does_asset_exist(asset)\n"
        "assert exists==os.path.isfile(disk),'WBP注册与原生磁盘包不一致，拒绝覆盖半成品'\n"
        "if not exists:\n"
        " stale=[os.path.splitext(disk)[0]+ext for ext in ('.uexp','.ubulk','.umap') if os.path.isfile(os.path.splitext(disk)[0]+ext)]\n"
        " assert not stale,('Widget包名存在未注册侧车，禁止重建覆盖',stale)\n"
        "if exists:\n"
        " w=unreal.EditorAssetLibrary.load_asset(asset)\n"
        " assert isinstance(w,unreal.WidgetBlueprint),'同名包非Widget Blueprint'\n"
        " tree=w.get_editor_property('widget_tree')\n"
        " assert tree is not None,'已存在Widget但无原生控件树'\n"
        " required="+repr(ids)+"\n"
        " assert all(tree.find_widget(name) is not None for name in required),'WBP控件树不完整，拒绝覆盖'\n"
        "print('SSLM_WBP_STATE:'+('EXISTS' if exists else 'MISSING'))\n"
    )
    preflight_state=read_ue_widget_state(editor_python(preflight))
    if preflight_state=="EXISTS":
        print("UI_WIDGET_12_CARD_EXISTING_NATIVE_PRESERVED",ASSET)
        return
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    evidence={"status":"pending","assetPath":ASSET,"parentClass":PARENT,
              "sourceSpec":str(SPEC.relative_to(ROOT)).replace("\\","/"),
              "widgetIdentifiers":ids,"uiMaterialCards":12,
              "monolithOperations":[],"finalCompilation":"pending",
              "uiBrushBinding":"pending_true_material_assets",
              "manualVisualApproval":"pending"}
    try:
        evidence["monolithOperations"].append(call("ui_query",{
            "action":"build_ui_from_spec",
            "params":{"asset_path":ASSET,"save_path":ASSET,"spec":spec}
        }))
        evidence["monolithOperations"].append(call("ui_query",{
            "action":"get_widget_tree",
            "params":{"asset_path":ASSET}
        }))
        editor_python(
            "import unreal\n"
            "w=unreal.EditorAssetLibrary.load_asset('"+ASSET+"')\n"
            "assert isinstance(w,unreal.WidgetBlueprint),'Monolith未生成原生Widget Blueprint'\n"
            "assert unreal.EditorAssetLibrary.save_loaded_asset(w,only_if_is_dirty=False)\n"
            "print('MATERIAL_LIBRARY_WIDGET_SAVED_PENDING_FINAL_COMPILE')\n"
        )
        evidence["status"]="widget_asset_saved_pending_final_compile_and_visual_review"
        if read_ue_widget_state(editor_python(preflight))!="EXISTS":
            raise RuntimeError("WBP创建后未取得物理包及83个原生控件回读证据")
    except Exception as exc:
        evidence["status"]="failed_do_not_auto_retry_mutation"
        evidence["error"]=str(exc)
        raise
    finally:
        REPORT.write_text(json.dumps(evidence,ensure_ascii=False,indent=2,default=str)+"\n",
                          encoding="utf-8")
    print("UI_WIDGET_12_CARD_MONOLITH_SAVED_WITHOUT_FINAL_COMPILE",ASSET)


if __name__=="__main__":
    main()
