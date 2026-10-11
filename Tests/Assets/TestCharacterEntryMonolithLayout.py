#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读检查真实Monolith角色页，防止放大面板后覆盖中央鼠标操作及生肖图像缺失。

输入为当前正式编辑器的本机MCP端点；不创建资产、不连接业务后端、不操作人工客户端。
实际控件、父类及命中合同任一不符均退出非零；源码扫描不代替此项真实资产检查。
"""
import argparse
import json
import urllib.request


def call(endpoint, tool, arguments):
    """解析传输层和动作层失败；收到HTTP响应不代表引擎动作成功。"""
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": tool, "arguments": arguments}}).encode("utf-8")
    request = urllib.request.Request(endpoint, body, {"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=60) as response:
        envelope = json.load(response)
    if "error" in envelope or envelope["result"].get("isError"):
        raise RuntimeError(json.dumps(envelope, ensure_ascii=False))
    blocks = [json.loads(c["text"]) for c in envelope["result"]["content"] if c["type"] == "text"]
    result = blocks[0]
    if any(result.get(key) is False for key in ("ok", "success", "bSuccess")):
        raise RuntimeError(json.dumps(result, ensure_ascii=False))
    return result


def widgets(tree):
    """递归读取真实控件树，不按目录存在推定绑定或布局已实现。"""
    result = {tree["name"]: tree}
    for child in tree.get("children", []):
        result.update(widgets(child))
    return result


def audit(endpoint):
    """期望尺寸来自用户指定倍数；保留独立文字/输入，背景面板不拦截鼠标。"""
    errors = []
    mount = "/DBAUIPack_Core/UI/"
    trees = {}
    for page in ("CharacterCreate", "CharacterSelect"):
        tree = call(endpoint, "ui_query", {"action": "get_widget_tree", "params": {
            "asset_path": mount + "Screens/WBP_DBA_UI_" + page}})
        trees[page] = widgets(tree["root"])
        if tree["parent_class"] != "DivineBeasts" + page + "Screen":
            errors.append(page + "原生父类错误")
        for name in ("BrandTitle", "BrandSubtitle", "TopRule"):
            if name in trees[page] and not trees[page][name]["visibility"].endswith("Collapsed"):
                errors.append(page + "/" + name + "仍显示旧文字/横线")
        surface = trees[page].get("PreviewDragSurface", {})
        if not surface.get("is_variable") or not surface.get("visibility", "").endswith("::Visible"):
            errors.append(page + "透明拖动区未形成可命中的命名绑定")
        if surface.get("slot", {}).get("anchors", {}).get("max_y") != 1:
            errors.append(page + "拖动区未随视口高度覆盖角色身体")
    for name, expected in (("HeroPanel", (536, 805)), ("CreationInfoPanel", (500, 540))):
        panel = trees["CharacterCreate"][name]
        offsets = panel["slot"]["offsets"]
        if (offsets["right"], offsets["bottom"]) != expected:
            errors.append(name + "未达到用户要求的放大尺寸")
        if not panel["visibility"].endswith("HitTestInvisible"):
            errors.append(name + "装饰背景截获中央鼠标")
    tree = call(endpoint, "ui_query", {"action": "get_widget_tree", "params": {
        "asset_path": mount + "Components/WBP_DBA_HeroChoice"}})
    choice = widgets(tree["root"])
    if choice.get("ChoicePortrait", {}).get("class") != "Image":
        errors.append("生肖卡片缺少真实图像控件")
    for name in ("ChoiceEmblem", "ChoiceTitle"):
        if not choice[name]["visibility"].endswith("Collapsed"):
            errors.append("生肖选项仍以文字代替图像：" + name)
    return {"Passed": not errors, "Errors": errors, "GPUVerified": False, "ClientVerified": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:9316/mcp")
    args = parser.parse_args()
    report = audit(args.endpoint)
    print(json.dumps(report, ensure_ascii=False))
    raise SystemExit(0 if report["Passed"] else 1)
