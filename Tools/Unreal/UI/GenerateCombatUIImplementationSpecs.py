# -*- coding: utf-8 -*-
"""构建战斗P0控件的Monolith MCP声明式规格，不生成或修改任何.uasset。

运行位置：DivineBeastsWorkspace/Tools/Unreal/UI；
输出位置：Saved/Monolith/CombatUIImplementationSpecs（仅JSON临时规格）。
三层规则：平台C++通用父类 -> 神兽联盟第三层DBAUIPack_Core中的视觉蓝图。
此脚本既不访问虚幻编辑器，也不声称蓝图编译、保存或运行。
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "Saved/Monolith/CombatUIImplementationSpecs"
OUTPUT.mkdir(parents=True, exist_ok=True)

# 与已有Monolith FrontEndMythicSpecs（神话前端规格）的线性颜色语义保持一致。
GOLD = "#A98750FF"
TEXT = "#D9D3C5FF"
MUTED = "#849397FF"
RED = "#A94442FF"
GREEN = "#4A906FFF"
PANEL = "#0A151EDB"


def pos(anchor, x, y, w, h, ax=0.0, ay=0.0, z=6):
    """所有坐标为逻辑像素；支持统一缩放、安全区与用户HUD布局设置。"""
    return {
        "anchorPreset": anchor,
        "position": {"x": x, "y": y},
        "size": {"x": w, "y": h},
        "alignment": {"x": ax, "y": ay},
        "zOrder": z,
    }


def node(kind, identity, slot=None, children=None, style=None, content=None):
    """生成一个Monolith UMG控件声明，内容只包括视觉标识和布局。"""
    result = {"type": kind, "id": identity}
    for key, value in [
        ("slot", slot),
        ("children", children),
        ("style", style),
        ("content", content),
    ]:
        if value is not None:
            result[key] = value
    return result


def text(identity, title, x, y, w=260, h=28, size=16, tint=TEXT):
    """默认文字不模拟任何真实Gameplay状态数据。"""
    return node(
        "TextBlock", identity, slot=pos("top_left", x, y, w, h),
        content={"text": title, "fontColor": tint, "fontSize": size, "wrapMode": "Auto"},
    )


def bg(identity, w, h):
    """半透明框架不遮挡对局场景，最终图标纹理由项目内容包决定。"""
    return node(
        "Border", identity, slot=pos("top_left", 0, 0, w, h, z=0),
        style={"background": PANEL, "visibility": "HitTestInvisible"},
    )


def progress(identity, x, y, w, h):
    """仅声明UMG进度条初始视觉，真实比例由项目只读ViewModel事件更新。"""
    return node("ProgressBar", identity, slot=pos("top_left", x, y, w, h),
                content={"percent": 0})


def tray(name, title, width=450, height=74):
    """状态图标区域使用容器复用，不为每一个效果生成固定业务对象。"""
    return [
        bg(name + "Frame", width, height),
        text(name + "Heading", title, 10, 7, width - 20, 24, 15, GOLD),
        node("WrapBox", name + "Items",
             slot=pos("top_left", 10, 34, width - 90, height - 40)),
        text(name + "OverflowText", "", width - 80, 37, 70, 24, 13, MUTED),
    ]


SPEC_TYPES = [
    # 资源字段、父类与焦点为明确声明；仅项目内容包有美术资源所有权。
    (
        "CastBar", "GamePlatformCastProgressWidget", "Combat",
        "施法/引导条：进度必须来自已授权施法源，无业务Tick。",
        [
            bg("CastBarFrame", 398, 88),
            text("CastNameText", "", 12, 8, 363, 29, 18, GOLD),
            progress("CastProgressBar", 12, 43, 373, 19),
            text("CastRemainingLabel", "", 278, 64, 100, 20, 12, MUTED),
        ],
    ),
    (
        "TargetFrame", "GamePlatformTargetFrameWidget", "Combat",
        "目标状态框：显示肖像、生命、盾条；可见性撤销必须清除旧数据。",
        [
            bg("TargetFrameBackground", 450, 134),
            node("Image", "TargetPortraitImage", slot=pos("top_left", 12, 16, 94, 94)),
            text("TargetDisplayName", "", 118, 12, 320, 28, 20, GOLD),
            progress("TargetHealthVisual", 118, 56, 312, 20),
            progress("TargetShieldVisual", 118, 86, 312, 14),
            text("TargetStateText", "", 118, 104, 300, 24, 12, MUTED),
        ],
    ),
    (
        "CombatAlert", "GamePlatformCombatAlertWidget", "Combat",
        "高优先关键战斗预警：显式来自战斗事实，不与Toast抢占。",
        [
            bg("CombatAlertBackground", 460, 112),
            text("AlertTitleText", "", 16, 13, 428, 42, 24, RED),
            text("AlertActionHint", "", 16, 59, 420, 26, 16, TEXT),
            text("AlertRemainingTime", "", 363, 89, 79, 17, 12, GOLD),
        ],
    ),
    (
        "BuffTray", "GamePlatformStatusEffectTrayWidget", "Combat",
        "增益图标的项目视觉分区，来源与排序仍用平台状态托盘。",
        tray("Buff", "增益状态"),
    ),
    (
        "DebuffTray", "GamePlatformStatusEffectTrayWidget", "Combat",
        "减益图标独立排列，危险类型需形状/文字与颜色双重可读。",
        tray("Debuff", "减益状态"),
    ),
    (
        "ControlAlert", "GamePlatformCombatAlertWidget", "Combat",
        "眩晕、沉默、定身等硬控制的专属警报；不猜测解除时间。",
        [
            bg("ControlAlertFrame", 315, 98),
            text("ControlHeading", "控制状态", 12, 8, 280, 27, 15, GOLD),
            text("AlertTitleText", "", 14, 38, 280, 37, 23, RED),
            text("ControlTimerText", "", 248, 78, 52, 18, 11, TEXT),
        ],
    ),
    (
        "StatusEffectIcon", "GamePlatformComponentWidget", "Combat",
        "平台通用状态图标可复用视觉原子；类型/层数/剩余时间由托盘授权快照填充。",
        [
            bg("EffectIconFrame", 64, 76),
            node("Image", "EffectIconImage", slot=pos("top_left", 5, 5, 54, 54)),
            text("EffectStackText", "", 37, 44, 22, 18, 12, TEXT),
            text("EffectTimerText", "", 3, 59, 57, 17, 10, MUTED),
        ],
    ),
    (
        "BossCastAlert", "GamePlatformCombatAlertWidget", "Combat",
        "首领关键技能预警：必须由游戏客户端已公开的机制事实驱动。",
        [
            bg("BossAlertFrame", 540, 130),
            text("BossAlertHeading", "首领机制", 18, 10, 490, 26, 16, GOLD),
            text("AlertTitleText", "", 18, 45, 500, 43, 28, RED),
            text("BossActionHint", "", 18, 98, 500, 26, 17, TEXT),
        ],
    ),
    (
        "TargetDebuffTray", "GamePlatformStatusEffectTrayWidget", "Combat",
        "仅显示观察者被授权看到的当前目标减益，不读取敌方隐藏ASC。",
        tray("TargetDebuff", "目标减益"),
    ),
    (
        "DispelBadge", "GamePlatformComponentWidget", "Combat",
        "驱散类别视觉标记：可驱散类别和本人可驱散资格要分别显示。",
        [bg("DispelFrame", 60, 60),
         node("Image", "DispelSymbolImage", slot=pos("top_left", 6, 6, 48, 48))],
    ),
    (
        "ImmunityBadge", "GamePlatformComponentWidget", "Combat",
        "免疫/不可打断视觉标记：不能仅靠红绿区分。",
        [bg("ImmunityFrame", 60, 60),
         node("Image", "ImmunitySymbolImage", slot=pos("top_left", 6, 6, 48, 48))],
    ),
    (
        "EffectOverflow", "GamePlatformComponentWidget", "Combat",
        "显示增益/减益过量收纳的+N入口；关键机制要有独立稳定显示区。",
        [bg("OverflowFrame", 65, 55),
         text("OverflowCountText", "", 9, 14, 47, 26, 18, GOLD)],
    ),
]

manifest = {
    "schemaVersion": 1,
    "creationToolRequired": "Monolith MCP",
    "project": "DivineBeastsArena（神兽联盟正式虚幻工程）",
    "contentRoot": "/DBAUIPack_Core/UI/Components",
    "sourceRoot": "GamePlatformUIClient（游戏平台UI客户端模块）",
    "assetState": "specification_only_not_compiled_or_saved",
    "specCount": len(SPEC_TYPES),
    "notes": [
        "仅生成声明式JSON，不会生产.uasset或触发编辑器编译。",
        "按AGENTS.md（工程规则）须先验证编辑器项目与Monolith连接。",
        "目标框可选的Typed BindWidget（强类型子控件）需使用真正派生蓝图而不是通用Image冒名绑定。",
        "蓝图事件图连接、输入焦点、移动端布局和业务数据适配尚需实际Editor核验。",
    ],
    "widgets": [],
}

for asset_suffix, class_suffix, domain, description, elements in SPEC_TYPES:
    name = "WBP_DBA_UI_" + asset_suffix
    spec = {
        "version": 1,
        "name": name,
        "parentClass": "/Script/GamePlatformUIClient." + class_suffix,
        "metadata": {
            "authoringTool": "Monolith MCP",
            "status": "spec_only",
            "purposeZh": description,
            "businessDomain": domain,
            "assetDirectory": "/DBAUIPack_Core/UI/Components",
            "mustCompileSaveReload": True,
        },
        "rootWidget": node(
            "CanvasPanel", "RootCanvas", children=elements,
            style={"visibility": "SelfHitTestInvisible"},
        ),
    }
    (OUTPUT / (asset_suffix + ".json")).write_text(
        json.dumps(spec, ensure_ascii=False, indent=2), encoding="utf-8",
    )
    manifest["widgets"].append({
        "name": name,
        "assetPath": "/DBAUIPack_Core/UI/Components/" + name,
        "parentClass": spec["parentClass"],
        "spec": "Saved/Monolith/CombatUIImplementationSpecs/" + asset_suffix + ".json",
        "status": "unverified_spec_only",
    })

(OUTPUT / "Manifest.json").write_text(
    json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8",
)
print("COMBAT_UI_SPEC_COUNT=" + str(len(SPEC_TYPES)))
print("COMBAT_UI_SPEC_OUTPUT=" + str(OUTPUT))
print("NO_UASSET_GENERATED=True")
