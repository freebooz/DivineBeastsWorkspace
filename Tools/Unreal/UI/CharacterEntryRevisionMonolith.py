#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""角色页增量布局修复：所有UE资产写入仅通过正式编辑器Monolith MCP。

第三层UI内容拥有生肖图集和控件；本工具只提交布局/导入动作，不重建页面、不改业务流程。
--apply才执行写入；修改前逐件备份三个现有Widget及记录哈希；动作失败立即停止并保留证据。
本工具不生成验收成功结论；保存后须独立重开编辑器、实际RHI、Cook和客户端核验。
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request

ROOT = Path(__file__).resolve().parents[3]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core"
MOUNT = "/DBAUIPack_Core/UI/"
CREATE = MOUNT + "Screens/WBP_DBA_UI_CharacterCreate"
SELECT = MOUNT + "Screens/WBP_DBA_UI_CharacterSelect"
CHOICE = MOUNT + "Components/WBP_DBA_HeroChoice"
ATLAS = MOUNT + "Textures/Zodiac/T_DBA_ZodiacChoiceAtlas_V1"
SOURCE = PACK / "SourceArt/UI/Zodiac/ZodiacChoiceAtlas_V1.png"


def rpc(endpoint, tool, arguments):
    """传输层、动作层及编译错误均阻断；不把isError=false当成资产验收。"""
    data = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": tool, "arguments": arguments}}).encode("utf-8")
    request = urllib.request.Request(endpoint, data, {"Content-Type": "application/json"})
    with urllib.request.urlopen(request, timeout=120) as response:
        envelope = json.load(response)
    if "error" in envelope or envelope["result"].get("isError"):
        raise RuntimeError(json.dumps(envelope, ensure_ascii=False))
    results = [json.loads(c["text"]) for c in envelope["result"]["content"] if c["type"] == "text"]
    result = results[0]
    if any(result.get(k) is False for k in ("ok", "success", "bSuccess")) or result.get("errors"):
        raise RuntimeError(json.dumps(result, ensure_ascii=False))
    return result


def operation(domain, action, **params):
    """操作域和正式资产身份明确，避免工具互换、隐式全包保存及内容树重建。"""
    return {"Tool": domain + "_query", "Arguments": {"action": action, "params": params}}


def slot(asset, widget, x, y, width, height, right=False, z=10):
    """固定逻辑像素；右锚点只改变面板位置，不缩放文字及输入。"""
    anchor = 1 if right else 0
    return operation("ui", "set_slot_property", asset_path=asset, widget_name=widget,
        anchors={"min_x": anchor, "min_y": 0, "max_x": anchor, "max_y": 0},
        offsets={"left": x, "top": y, "right": width, "bottom": height},
        alignment={"x": anchor, "y": 0}, z_order=z, auto_size=False)


def property_write(asset, widget, key, value):
    """仅对已核对字段写入；raw_mode用于UE5.8复合属性，不绕过工程边界。"""
    return operation("ui", "set_widget_property", asset_path=asset, widget_name=widget,
                     property_name=key, value=value, raw_mode=True, compile=False)


def requests():
    """本轮确定性增量动作；尺寸基线268×322/200×216，不读取已经放大的值再次乘倍数。"""
    ops = []
    for asset in (CREATE, SELECT):
        for name in ("BrandTitle", "BrandSubtitle", "TopRule"):
            ops.append(operation("ui", "remove_widget", asset_path=asset, widget_name=name, compile=False))
        ops.append(operation("ui", "set_widget_is_variable", wbp_path=asset,
                             widget_name="PreviewDragSurface", is_variable=True))
        ops += [operation("ui", "set_slot_property", asset_path=asset, widget_name="PreviewDragSurface",
                          anchors={"min_x": .5, "min_y": 0, "max_x": .5, "max_y": 1},
                          offsets={"left": 0, "top": 70, "right": 800, "bottom": 142},
                          alignment={"x": .5, "y": 0}, z_order=1),
                property_write(asset, "PreviewDragSurface", "Visibility", "Visible"),
                property_write(asset, "PreviewDragSurface", "BrushColor", "(R=1,G=1,B=1,A=0)")]
    ops += [slot(CREATE, "HeroPanel", 24, 96, 536, 805, z=2),
            slot(CREATE, "CreationInfoPanel", -24, 96, 500, 540, right=True, z=2),
            property_write(CREATE, "HeroPanel", "Visibility", "HitTestInvisible"),
            property_write(CREATE, "CreationInfoPanel", "Visibility", "HitTestInvisible"),
            property_write(CREATE, "HeroHeading", "Visibility", "Collapsed"),
            slot(CREATE, "HeroInstruction", 84, 174, 416, 24),
            slot(CREATE, "HeroChoices", 74, 220, 446, 600),
            property_write(CREATE, "HeroChoices", "InnerSlotPadding", "(X=10,Y=10)"),
            property_write(CREATE, "HeroChoices", "WrapSize", 446),
            property_write(CREATE, "HeroChoices", "bExplicitWrapSize", True),
            slot(CREATE, "CreationInfoTitle", -84, 174, 380, 30, right=True),
            slot(CREATE, "CreationInfoBody", -84, 224, 380, 184, right=True),
            slot(CREATE, "BackToCharacterSelectButton", -84, 492, 240, 63, right=True),
            property_write(CHOICE, "ChoiceSize", "WidthOverride", 140),
            property_write(CHOICE, "ChoiceSize", "HeightOverride", 140),
            property_write(CHOICE, "ChoiceEmblem", "Visibility", "Collapsed"),
            property_write(CHOICE, "ChoiceTitle", "Visibility", "Collapsed"),
            property_write(CHOICE, "ChoiceSubtitle", "Visibility", "Collapsed"),
            operation("ui", "add_widget", asset_path=CHOICE, widget_class="Image",
                      widget_name="ChoicePortrait", parent_name="ChoiceBody", compile=False),
            operation("ui", "set_widget_is_variable", wbp_path=CHOICE,
                      widget_name="ChoicePortrait", is_variable=True),
            operation("ui", "set_slot_property", asset_path=CHOICE, widget_name="ChoicePortrait",
                      h_align="Center", v_align="Center", padding={"left": 0, "top": 0, "right": 0, "bottom": 0}),
            operation("ui", "set_image", asset_path=CHOICE, widget_name="ChoicePortrait",
                      texture_path=ATLAS, tint_color="#FFFFFFFF"),
            property_write(CHOICE, "ChoicePortrait", "Brush.ImageSize", "(X=112,Y=112)"),
            property_write(CHOICE, "ChoicePortrait", "Visibility", "HitTestInvisible")]
    for asset in (CHOICE, CREATE, SELECT):
        ops += [operation("ui", "dump_blueprint_compile_log", asset_path=asset),
                operation("blueprint", "save_asset", asset_path=asset)]
    return ops


def introduction_requests():
    """右面板独立滚动介绍；动态文本默认空，只由原生VM的事件快照填写。"""
    ops = [operation("ui", "remove_widget", asset_path=CREATE, widget_name="CreationInfoBody", compile=False),
           operation("ui", "set_text", asset_path=CREATE, widget_name="CreationInfoTitle", text="角色与技能介绍", font_size=18),
           operation("ui", "add_widget", asset_path=CREATE, widget_class="ScrollBox", widget_name="HeroOverviewScroll", parent_name="RootCanvas", compile=False),
           slot(CREATE, "HeroOverviewScroll", -70, 214, 410, 260, right=True),
           operation("ui", "add_widget", asset_path=CREATE, widget_class="VerticalBox", widget_name="HeroOverviewBody", parent_name="HeroOverviewScroll", compile=False),
           operation("ui", "set_slot_property", asset_path=CREATE, widget_name="HeroOverviewBody", h_align="Fill", v_align="Top")]
    for name, value, size in (("HeroOverviewStatusText", "", 12), ("HeroIntroductionHeading", "角色介绍", 16),
                              ("HeroIntroductionText", "", 14), ("SkillIntroductionHeading", "技能介绍", 16),
                              ("SkillIntroductionText", "", 14)):
        ops += [operation("ui", "add_widget", asset_path=CREATE, widget_class="TextBlock", widget_name=name, parent_name="HeroOverviewBody", compile=False),
                operation("ui", "set_text", asset_path=CREATE, widget_name=name, text=value, font_size=size, text_color="#D6DCE3FF"),
                property_write(CREATE, name, "AutoWrapText", True),
                property_write(CREATE, name, "WrapTextAt", 390),
                operation("ui", "set_slot_property", asset_path=CREATE, widget_name=name, h_align="Fill", v_align="Top", padding={"left": 0, "top": 2, "right": 6, "bottom": 8})]
        if name not in ("HeroIntroductionHeading", "SkillIntroductionHeading"):
            ops.append(operation("ui", "set_widget_is_variable", wbp_path=CREATE, widget_name=name, is_variable=True))
    return ops + [operation("ui", "dump_blueprint_compile_log", asset_path=CREATE), operation("blueprint", "save_asset", asset_path=CREATE)]


def apply_introductions(endpoint):
    """与旧图集修改分开留证，不重导入已批准RGB、不重放旧面板倍率动作。"""
    evidence = ROOT / "Saved/Validation/CharacterEntryFollowUp20261012/Introductions"
    backup = evidence / "Before"
    backup.mkdir(parents=True, exist_ok=False)
    source = PACK / "Content/UI/Screens/WBP_DBA_UI_CharacterCreate.uasset"
    shutil.copy2(source, backup / source.name)
    (evidence / "Before.json").write_text(json.dumps({"Source": str(source), "SHA256": hashlib.sha256(source.read_bytes()).hexdigest()}, indent=2), encoding="utf-8")
    log = []
    for index, item in enumerate(introduction_requests()):
        result = rpc(endpoint, item["Tool"], item["Arguments"])
        log.append({"Request": item, "Result": result})
        (evidence / "Actions.json").write_text(json.dumps(log, ensure_ascii=False, indent=2), encoding="utf-8")
        print(index, item["Arguments"]["action"], flush=True)


def main():
    """锁定正确工程及版本后逐条执行；证据保存动作原始返回而非人工写通过数字。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:9316/mcp")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--introductions", action="store_true", help="仅增量添加创建角色介绍滚动区，不重放旧图集/面板修复")
    args = parser.parse_args()
    if not args.apply:
        print(json.dumps(introduction_requests() if args.introductions else requests(), ensure_ascii=False, indent=2))
        return
    status = rpc(args.endpoint, "monolith_status", {})
    if status.get("project_name") != "DivineBeastsArena" or status.get("version") != "0.23.0":
        raise RuntimeError("当前Monolith工程或锁定版本不符")
    if args.introductions:
        apply_introductions(args.endpoint)
        return
    evidence = ROOT / "Saved/Validation/CharacterEntryRevision20261011"
    backup = evidence / "Before"
    backup.mkdir(parents=True, exist_ok=False)
    files = [PACK / "Content" / (path[len('/DBAUIPack_Core/'):] + ".uasset")
             for path in (CREATE, SELECT, CHOICE)]
    records = []
    for source in files:
        if not source.is_file():
            raise RuntimeError("真实Widget缺失：" + str(source))
        target = backup / source.name
        shutil.copy2(source, target)
        records.append({"File": str(source), "Backup": str(target),
                        "SHA256": hashlib.sha256(source.read_bytes()).hexdigest()})
    (evidence / "Before.json").write_text(json.dumps(records, ensure_ascii=False, indent=2), encoding="utf-8")
    manifest = {"Files": [{"Source": str(SOURCE.relative_to(ROOT)).replace('\\', '/'),
                           "Destination": ATLAS, "Role": "Color",
                           "SHA256": hashlib.sha256(SOURCE.read_bytes()).hexdigest()}]}
    (evidence / "AtlasImportManifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # 复用既有严格颜色导入合同，不手改PNG、不直接写UE二进制。
    import importlib.util
    spec = importlib.util.spec_from_file_location("ui_import", Path(__file__).with_name("ImportDBAThemeTexturesMonolith.py"))
    policy = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(policy)
    import_ops = [operation(*item["tool"].split('.'), **item["params"]) for item in policy.prepare(manifest, ROOT)]
    log = []
    for index, item in enumerate(import_ops + requests()):
        try:
            result = rpc(args.endpoint, item["Tool"], item["Arguments"])
            log.append({"Request": item, "Result": result})
            print(index, item["Tool"], item["Arguments"]["action"], flush=True)
        finally:
            (evidence / "Actions.json").write_text(json.dumps(log, ensure_ascii=False, indent=2), encoding="utf-8")
    print("MONOLITH_ACTIONS_SAVED; RELOAD_RHI_COOK_CLIENT_STILL_REQUIRED")


if __name__ == "__main__":
    main()
