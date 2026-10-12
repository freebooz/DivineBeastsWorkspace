#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群演示UI：Monolith真实UE5.8资产制作/编译/回读。

--inspect：只读检查C++父类身份、规格、必要的17个控件标识及UI视觉合同；不生成二进制。
--author：需锁定UE5.8主工程的在线Monolith与已经编译加载的UDivineBeastsBirdFlockDemoWidget反射类。
执行成功还需验证原生WidgetTree、BindWidgetOptional控件名、蓝图编译/保存/重开与输入焦点，
自动记录不等于最终人工视觉审核签字。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
SPEC=ROOT/"Docs/Implementation/BirdFlockSplineFlight/BirdFlockDemoUISpec.json"
ASSET="/Game/Development/PluginDemos/UI/WBP_DBA_PluginDemoBirdFlock"
PARENT="/Script/DivineBeastsDemoClient.DivineBeastsBirdFlockDemoWidget"
REPORT=ROOT/"Saved/Validation/BirdFlockSplineFlight/BirdDemoUIAuthoring.json"

REQUIRED={
 "SpeciesCombo","FlightModeCombo","BirdCountInput","GlideBlendInput",
 "SeparationInput","AlignmentInput","CohesionInput","ApplyBirdCountButton",
 "ApplySpeciesButton","ApplyFlightModeButton","ApplyFlockingButton",
 "ApplyGlideButton","StartBatchButton","StopBatchButton","ResetButton",
 "BackButton","FeedbackText","RealGpuCountText"
}


def nodes(tree):
    if isinstance(tree,dict):
        if isinstance(tree.get("id"),str):
            yield tree["id"]
        for value in tree.values():
            yield from nodes(value)
    elif isinstance(tree,list):
        for x in tree:yield from nodes(x)


def load_spec():
    if not SPEC.is_file():
        raise RuntimeError("鸟群Monolith UI规格不存在："+str(SPEC))
    data=json.loads(SPEC.read_text(encoding="utf-8"))
    if data.get("parentClass")!=PARENT or data.get("name")!="WBP_DBA_PluginDemoBirdFlock":
        raise RuntimeError("UI反射父类或资产身份与项目正式代码不一致")
    actual=list(nodes(data["rootWidget"]))
    if len(actual)!=len(set(actual)):
        raise RuntimeError("UI Widget树存在重复命名控件")
    missing=sorted(REQUIRED-set(actual))
    if missing:raise RuntimeError("缺少Native绑定控件："+",".join(missing))
    return data


def run():
    parser=argparse.ArgumentParser()
    parser.add_argument("--inspect",action="store_true")
    parser.add_argument("--author",action="store_true")
    args=parser.parse_args()
    spec=load_spec()
    if args.inspect:
        print(json.dumps({"status":"source_spec_checked_only",
                          "asset_path":ASSET,"parent":PARENT,
                          "required_controls":len(REQUIRED),"root_count":len(list(nodes(spec["rootWidget"]))),
                          "actual_widget_asset":"not_verified"},ensure_ascii=False))
        return
    if not args.author:
        raise RuntimeError("请显式选择--inspect或--author，不允许默认修改UE资产")
    if os.environ.get("BIRD_SPLINE_UI_AUTHOR")!="1":
        raise RuntimeError("未授权UI资产写入：需明确BIRD_SPLINE_UI_AUTHOR=1")
    from BirdFlockMonolithProtocol import call,editor_python
    # 作者前核对真实编辑器、C++模块与蓝图父类；不允许回退成任意UserWidget空资产。
    editor_python(
        "import unreal\n"
        "project=unreal.Paths.get_project_file_path().replace('\\\\','/')\n"
        "assert project.endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject'),project\n"
        "assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\n"
        "c=unreal.load_class(None,'"+PARENT+"')\n"
        "assert c is not None,'必须先载入已真实编译的鸟群演示Widget父类'\n"
        "print('BIRD_DEMO_UI_PARENT_READY',c.get_path_name())\n")
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    result={"status":"pending","asset":ASSET,"required_widget_names":sorted(REQUIRED),
            "monolith_operations":[],"visual_human_approval":"pending"}
    try:
        # Monolith的UI声明式能力负责Widget树创建，禁止Python直写UWidgetBlueprint绕过规定通道。
        result["monolith_operations"].append(call("ui_query",{
            "action":"build_ui_from_spec",
            "params":{"asset_path":ASSET,"save_path":ASSET,"spec":spec}
        }))
        result["monolith_operations"].append(call("ui_query",{
            "action":"compile_widget","params":{"asset_path":ASSET}
        }))
        result["monolith_operations"].append(call("ui_query",{
            "action":"get_widget_tree","params":{"asset_path":ASSET}
        }))
        editor_python(
            "import unreal\n"
            "p='"+ASSET+"'\n"
            "w=unreal.EditorAssetLibrary.load_asset(p)\n"
            "assert isinstance(w,unreal.WidgetBlueprint),'Monolith未真正生成Widget蓝图'\n"
            "assert unreal.EditorAssetLibrary.save_loaded_asset(w,only_if_is_dirty=False)\n"
            "assert unreal.EditorAssetLibrary.load_blueprint_class(p) is not None\n"
            "print('BIRD_DEMO_UI_REAL_ASSET_RELOADED',p)\n")
        result["status"]="widget_saved_requires_runtime_manual_acceptance"
    except Exception as exc:
        result["status"]="failed"
        result["error"]=str(exc)
        raise
    finally:
        REPORT.write_text(json.dumps(result,ensure_ascii=False,indent=2,default=str)+"\n",
                          encoding="utf-8")
    print("BIRD_DEMO_UI_MONOLITH_ASSET_SAVED_PENDING_VISUAL_APPROVAL")


if __name__=="__main__":
    run()
