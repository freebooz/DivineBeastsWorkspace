#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""为 Monolith MCP 准备神兽联盟主题资产的确定性操作队列。

只生成 JSON 请求数据，不调用 UE、不生成 .uasset，也不更新通过状态。
必须由真实 Unreal Editor + Monolith MCP 逐项执行并根据实际 CDO Schema 配置样式Brush；
本脚本只负责安全排序和可靠身份，不对不可知的Slate结构字段作猜测。
"""
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SPEC = (ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
        "DBAUIPack_Core/Docs/UIThemeMonolithAuthoringSpec_20261010.json")
OUT = ROOT / "Saved/Monolith/UITheme20261010/AuthoringRequests.json"

def monolith(action: str, params: dict) -> dict:
    """构造可直接作为 blueprint_query 参数使用的请求（仍须人工逐项审查）。"""
    return {
        "Tool": "blueprint_query",
        "Arguments": {"action": action, "params": params},
        "State": "PendingEditor",
    }

def style_class_path(asset_path: str) -> str:
    """资产长包路径转为 Blueprint GeneratedClass 软引用，不创建资产。"""
    asset_name = asset_path.rsplit("/", 1)[-1]
    return f"{asset_path}.{asset_name}_C"

def build() -> dict:
    data = json.loads(SPEC.read_text(encoding="utf-8"))
    theme = data["ThemeDefinition"]
    styles = data["StyleBlueprints"]
    root = "/DBAUIPack_Core/UI/"
    assert data["Project"] == "Game/DivineBeastsArena.uproject"
    assert theme["AssetPath"].startswith(root + "Themes/")
    assert theme["Class"] == "/Script/GamePlatformUIClient.GamePlatformUIThemeDefinition"
    assert len(styles) == 5 and len({e["AssetPath"] for e in styles}) == 5
    assert len({e["StyleId"] for e in styles}) == 5
    assert set(theme["RequiredStyleIds"]) == {e["StyleId"] for e in styles}
    assert all(e["AssetPath"].startswith(root + "Styles/") for e in styles)
    assert set(e["ParentClass"] for e in styles) == {
        "/Script/CommonUI.CommonButtonStyle",
        "/Script/CommonUI.CommonTextStyle",
        "/Script/CommonUI.CommonBorderStyle",
    }
    operations = []
    for style in styles:
        path = style["AssetPath"]
        operations.extend([
            {
                **monolith("create_blueprint", {
                    "save_path": path,
                    "parent_class": style["ParentClass"].split(".")[-1],
                }),
                "Phase": "1.创建引擎原生样式蓝图",
                "ExpectedPath": path,
            },
            {
                **monolith("describe_cdo_schema", {"asset_path": path}),
                "Phase": "2.检查真实CDO样式结构",
                "ExpectedPath": path,
                "AfterZh": "用真实字段架构，经Monolith写入纹理画刷、Normal/Hovered/Pressed/Disabled状态及字体属性；请勿盲写结构体。",
            },
            {
                **monolith("compile_blueprint", {"asset_path": path}),
                "Phase": "3.真实蓝图编译",
                "ExpectedPath": path,
            },
            {
                **monolith("save_asset", {"asset_path": path}),
                "Phase": "4.保存主题样式蓝图",
                "ExpectedPath": path,
            },
        ])
    # 先生成开发对照样式的真实Monolith请求；开发资源不能进入正式主题目录或Cook。
    dev_styles = data.get("DevelopmentStyleBlueprints", [])
    assert len(dev_styles) == 2
    assert all(x["AssetPath"].startswith("/Game/Development/UITheme/Styles/") for x in dev_styles)
    for item in dev_styles:
        path = item["AssetPath"]
        operations.extend([
            {
                **monolith("create_blueprint", {
                    "save_path": path,
                    "parent_class": item["ParentClass"].split(".")[-1],
                }),
                "Phase": "开发主题A/B：创建中性样式类",
                "ExpectedPath": path,
                "ShippingAllowed": False,
            },
            {
                **monolith("describe_cdo_schema", {"asset_path": path}),
                "Phase": "开发主题A/B：确认CDO字段后才写中性画刷",
                "ExpectedPath": path,
                "ShippingAllowed": False,
            },
            {
                **monolith("compile_blueprint", {"asset_path": path}),
                "Phase": "开发主题A/B：编译中性样式类",
                "ExpectedPath": path,
                "ShippingAllowed": False,
            },
            {
                **monolith("save_asset", {"asset_path": path}),
                "Phase": "开发主题A/B：保存中性样式类",
                "ExpectedPath": path,
                "ShippingAllowed": False,
            },
        ])

    buttons, texts, borders, required = [], [], [], []
    for item in styles:
        asset = item["AssetPath"]
        sid = item["StyleId"]
        rule = {"StyleId": sid, "Scope": theme["Scope"], "Priority": 0}
        entry = {"Rule": rule, "StyleClass": style_class_path(asset)}
        if ".Button." in sid:
            target, kind = buttons, "Button"
        elif ".Text." in sid:
            target, kind = texts, "Text"
        else:
            target, kind = borders, "Border"
        target.append(entry)
        required.append({"Kind": kind, "StyleId": sid})
    assert len(buttons) == 2 and len(texts) == 2 and len(borders) == 1
    operations.extend([
        {
            **monolith("seed_data_asset", {
                "save_path": theme["AssetPath"],
                "class_name": theme["Class"],
                "tree": {
                    "LogicalId": theme["LogicalId"],
                    "DataVersion": theme["DataVersion"],
                    "Buttons": buttons,
                    "Texts": texts,
                    "Borders": borders,
                    "RequiredStyles": required,
                },
                "strict": True,
                "read_back_values": True,
            }),
            "Phase": "5.正式主题主资产（五个语义键）",
            "ExpectedPath": theme["AssetPath"],
            "PrerequisiteZh": "必须先完整验证全部五个样式蓝图可加载、CDO状态有效，再写入正式主题。",
        },
        {
            **monolith("get_cdo_properties", {"asset_path": theme["AssetPath"]}),
            "Phase": "6.主资产回读",
            "ExpectedPath": theme["AssetPath"],
        },
    ])
    # 中性开发主题仅是视觉对照：样式语义与正式主题完全相同，但Primary/Secondary
    # 引用开发中性按钮、Panel使用开发中性边框，Text复用已经由Monolith审核的项目文字样式。
    # 对照组不覆盖正式DataAsset/默认ThemeId，也不能被Cook进发行包。
    neutral_theme = data["DevelopmentTheme"]
    neutral_button = style_class_path(dev_styles[0]["AssetPath"])
    neutral_border = style_class_path(dev_styles[1]["AssetPath"])
    neutral_buttons = [{"Rule": dict(entry["Rule"]), "StyleClass": neutral_button} for entry in buttons]
    neutral_borders = [{"Rule": dict(entry["Rule"]), "StyleClass": neutral_border} for entry in borders]
    operations.append({
        **monolith("seed_data_asset", {
            "save_path": neutral_theme["AssetPath"],
            "class_name": theme["Class"],
            "tree": {
                "LogicalId": {"Namespace": "dba.ui.theme", "Name": "neutral_dev", "LogicalVersion": 1},
                "DataVersion": theme["DataVersion"],
                "Buttons": neutral_buttons,
                "Texts": texts,
                "Borders": neutral_borders,
                "RequiredStyles": required,
            },
            "strict": True,
            "read_back_values": True,
        }),
        "Phase": "开发主题A/B：只创建NeverCook中性主题定义",
        "ExpectedPath": neutral_theme["AssetPath"],
        "ShippingAllowed": False,
        "PrerequisiteZh": "先完成开发两类样式CDO并核对原生字体，禁止将此资产填入正式DefaultThemeDefinitionId",
    })
    operations.append({
        **monolith("get_cdo_properties", {"asset_path": neutral_theme["AssetPath"]}),
        "Phase": "开发主题A/B：回读中性主题Definition",
        "ExpectedPath": neutral_theme["AssetPath"],
        "ShippingAllowed": False,
    })
    for path in [theme["AssetPath"], neutral_theme["AssetPath"]] + [e["AssetPath"] for e in styles + dev_styles]:
        operations.append({
            **monolith("get_saved_asset_state", {"asset_path": path}),
            "Tool": "project_query",
            "Phase": "7.已保存资产状态回读",
            "ExpectedPath": path,
        })
    steps = [
        "先以Monolith核对UE5.8正式项目、GamePlatformUIClient和DivineBeastsUIClient反射模块已加载",
        "蓝图创建前用project.search确认没有已存在同名资产，禁止覆盖并行任务",
        "每个Style蓝图创建后先inspect CDO字段，按已锁定sRGB→Linear规则设置画刷与文本属性",
        "检查按钮四种视觉态、中文字体/字号、九宫格边框及SlateBrush资源路径，缺项不得种下Theme",
        "主题DataAsset回读包含5个不重复StyleId及非空GeneratedClass软引用，LogicalId和DataVersion必须准确",
        "用Monolith的ui.build_ui制作WBP_DBA_UI_StandardPanel，项目Panel原生父类/四个命名节点按另附Spec检查",
        "用Monolith回读已有登录/选角/创建蓝图的ThemeBindings，必要时只设置类默认属性，不修改控件树或密码框",
        "开发对照主题和样式仅放/Game/Development/UITheme/；确保仍受到NeverCook保护",
        "所有资产真实编译、保存、关闭并重新打开回读成功后，才由项目配置启用DefaultThemeDefinitionId",
        "最后统一执行自动化、真实客户端/专服Cook与性能检查，此阶段不提前测试",
    ]
    return {
        "SchemaVersion": 1,
        "State": "PendingEditor",
        "DescriptionZh": "Monolith主题资源制作请求队列；仅为已批准代码和资源的操作输入，不是资产或编译成功证据",
        "Workspace": "DivineBeastsWorkspace",
        "Project": data["Project"],
        "ThemeId": theme["PrimaryAssetId"],
        "StyleCount": len(styles),
        "DevelopmentStyleCount": len(dev_styles),
        "DevelopmentThemeId": neutral_theme["AssetPath"],
        "ExpectedStyleIds": theme["RequiredStyleIds"],
        "Operations": operations,
        "RemainingStepsZh": steps,
        "NonExecutableUntilEditorIsReady": True,
        "SourceSpec": SPEC.relative_to(ROOT).as_posix(),
    }

def main() -> None:
    result = build()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("MONOLITH_REQUESTS", OUT.relative_to(ROOT).as_posix())
    print("STYLE_DEFINITIONS", result["StyleCount"], "REQUEST_ACTIONS", len(result["Operations"]))
    print("EXECUTED=NO ASSETS_CREATED=NO AUTOMATION_RUN=NO")

if __name__ == "__main__":
    main()
